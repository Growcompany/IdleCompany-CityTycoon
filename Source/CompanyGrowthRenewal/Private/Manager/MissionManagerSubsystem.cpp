#include "Manager/MissionManagerSubsystem.h"
#include "Manager/ContentGuideStep.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Manager/MissionGuidePhases.h"
#include "Manager/GuideGestureRules.h"
#include "Manager/DevPresetSeeder.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/GoalBoardSubsystem.h"
#include "Manager/OverseasActions.h"
#include "Manager/QuestSubsystem.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/UIManagerSubsystem.h"
#include "UI/UIBase.h"
#include "UI/Panel/BuildOpenWidget.h"
#include "UI/Panel/BuildModalWidget.h"
#include "UI/Panel/OfficeWorkstationPanelWidget.h"
#include "UI/Panel/PlotPurchasePanelWidget.h"
#include "UI/Panel/ProjectReportWidget.h"
#include "UI/Panel/BuildingManagePanelWidget.h"
#include "UI/Panel/FactoryPanelWidget.h"
#include "UI/Panel/WorldMapPanelWidget.h"
#include "UI/Panel/CountryDetailWidget.h"
#include "UI/Element/Common/PanelHeaderWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "UI/Element/Trade/CountryNameWidget.h"
#include "UI/Element/Buttons/CostActionButtonWidget.h"
#include "UI/Panel/BuildPlacementPanelWidget.h"
#include "Entity/Building/BuildingBaseActor.h"
#include "UI/Panel/OfficeMainWidget.h"
#include "UI/Panel/OfficeRecruitmentModalWidget.h"
#include "Office/OfficeRecruitmentFlowSubsystem.h"
#include "UI/Panel/EmployeeGachaPresentationWidget.h"
#include "UI/Panel/RewardRevealPresentationWidget.h"
#include "Data/TutorialMissionCompletionRules.h"
#include "Data/TutorialMissionAtomicCommitRules.h"
#include "Data/TutorialMissionRestoreRules.h"
#include "Data/TutorialRevenueCompletionRules.h"
#include "Data/TutorialCompletionStateRules.h"
#include "Data/TutorialDerivedMissionRules.h"
#include "Data/TutorialEnhancementGateRules.h"
#include "Manager/PanelIntroSubsystem.h"
#include "Manager/EmployeeEquipmentManagerSubsystem.h"
#include "Manager/SpawnManager.h"
#include "Entity/Plot/CityPlotActor.h"
#include "UI/Panel/MenuPanelWidget.h"
#include "UI/Panel/ShopPanelWidget.h"
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
#include "Manager/MineManager.h"
#include "Manager/WorldFactoryManager.h"
#include "Manager/ProductionOrderManager.h"
#include "Manager/ManufacturingGuideCountryRules.h"
#include "Table/ProductRecipeTable.h"
#include "Manager/ManufacturingDevelopmentManager.h"
#include "Manager/OfficeStageProgressManager.h"
#include "UI/Panel/BuildingTraitGachaPanelWidget.h"
#include "UI/Panel/EnhanceStarforceModalWidget.h"
#include "Manager/CityAcquisitionManager.h"
#include "Manager/EmployeeManager.h"
#include "Core/CGGameInstance.h"
#include "GameMode/CGGameModeBase.h"
#include "Utils/TutorialDisciplineSeed.h"
#include "UI/Panel/InGameLayerWidget.h"
#include "UI/Panel/OfficeLayerWidget.h"
#include "UI/Panel/EmployeeWindowWidget.h"
#include "UI/Panel/OfficeEmployeePanelWidget.h"
#include "UI/Panel/HQManagePanelWidget.h"
#include "UI/Panel/HQLevelUpCelebrationWidget.h"
#include "UI/Panel/CityCompanyManageWidget.h"
#include "UI/Panel/CityCompanyInfoWidget.h"
#include "UI/Panel/EquipmentGachaPanelWidget.h"
#include "UI/Panel/GachaPanelBaseWidget.h"
#include "UI/Panel/OfficeDecorationPanelWidget.h"
#include "UI/Element/Common/ConfirmCancelWidget.h"
#include "Office/DecorationTypes.h"
#include "UI/HUD/MissionTrackerWidget.h"
#include "UI/HUD/GoalTrackerWidget.h"
#include "UI/HUD/MissionGuideOverlayWidget.h"
#include "UI/HUD/RewardClaimSplashWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Player/Components/PlacementHandler.h"
#include "Player/PlayerCamera.h"
#include "Office/OfficeManager.h"
#include "Office/WorkstationActorBase.h"
#include "UI/Element/Building/UpgradeSlot.h"
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
#include "MoviePlayer.h"

namespace
{
	// 모드는 확정 직후 프리뷰를 옮기는 프레임에도 유지된다(연속 배치) — 프리뷰 유무보다 안정적이다
	EPlacementMode GetTutorialPlacementMode(const UGameInstance* GI)
	{
		APlayerController* PC = GI ? GI->GetFirstLocalPlayerController() : nullptr;
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		const UPlacementHandler* Handler = Pawn ? Pawn->FindComponentByClass<UPlacementHandler>() : nullptr;
		return Handler ? Handler->GetCurrentPlacementMode() : EPlacementMode::None;
	}

	// 튜토리얼에서 철거한 회사가 있던 부지 ― 부지 설명 걸음의 대상
	ACityPlotActor* FindTutorialClearedPlot(const UWorld* World)
	{
		const UCityAcquisitionManager* AcqMgr = World ? World->GetSubsystem<UCityAcquisitionManager>() : nullptr;
		const USpawnManager* Spawns = World ? World->GetSubsystem<USpawnManager>() : nullptr;
		const int32 ClearedKey = AcqMgr ? AcqMgr->FindFirstCompanyKeyInState(EAcqState::Cleared) : INDEX_NONE;
		return Spawns && ClearedKey != INDEX_NONE ? Spawns->FindPlotByOccupantKey(ClearedKey) : nullptr;
	}

	int32 CountPlacedSeats(UWorld* World)
	{
		int32 Seats = 0;
		if (UOfficeManager* Office = World ? World->GetSubsystem<UOfficeManager>() : nullptr)
		{
			for (AWorkstationActorBase* Desk : Office->GetPlacedWorkstations())
			{
				Seats += IsValid(Desk) ? Desk->GetChairCount() : 0;
			}
		}
		return Seats;
	}

	bool HasVacantWorkstation(UWorld* World)
	{
		if (UOfficeManager* Office = World ? World->GetSubsystem<UOfficeManager>() : nullptr)
		{
			for (AWorkstationActorBase* Desk : Office->GetPlacedWorkstations())
			{
				if (IsValid(Desk) && Desk->FindEmptyAssignmentSlot() >= 0) return true;
			}
		}
		return false;
	}

	// 연출 중 숨겨 둔 버튼(뽑기 결과 [확인] 등)은 보일 때까지 기다림으로 둔다 — 안전망이 게이트만 푸는 것보다 문구가 정확하다
	UWidget* VisibleOrNull(UWidget* Widget)
	{
		return Widget && Widget->IsVisible() ? Widget : nullptr;
	}

	// 채용 직원이 장비(채용 선물)를 끼고 나올 수 있다 — 미션 시작 때 끼고 있던 장비를 기준으로 남기고, 그 밖의 장비를 끼면 체험한 것으로 본다
	void CollectEquippedInBuilding(const UGameInstance* GI, int32 BuildingIndex, TArray<FGuid>& OutIds)
	{
		const UEmployeeEquipmentManagerSubsystem* Equipment = GI ? GI->GetSubsystem<UEmployeeEquipmentManagerSubsystem>() : nullptr;
		const UEmployeeManager* Employees = GI ? GI->GetSubsystem<UEmployeeManager>() : nullptr;
		if (!Equipment || !Employees || BuildingIndex == INDEX_NONE)
		{
			return;
		}
		for (const FEmployeeInstance& Employee : Employees->GetEmployeesInBuilding(BuildingIndex))
		{
			for (int32 PartIndex = 0; PartIndex < static_cast<int32>(EEmployeeEquipmentSlot::Count); ++PartIndex)
			{
				if (const FEmployeeEquipmentInstance* Item = Equipment->GetEquippedInstance(Employee.EmployeeID, static_cast<EEmployeeEquipmentSlot>(PartIndex)))
				{
					OutIds.Add(Item->InstanceId);
				}
			}
		}
	}


	// 대상이 스크롤 목록 안에 있으면 그 스크롤 상자 — 카드가 화면 밖에 걸쳐 있으면 짚어도 누를 수 없다
	UScrollBox* FindGuideScrollBox(UWidget* Widget)
	{
		UWidget* Cur = Widget;
		for (int32 Hop = 0; Cur && Hop < 32; ++Hop)
		{
			if (UPanelWidget* Parent = Cur->GetParent())
			{
				if (UScrollBox* Scroll = Cast<UScrollBox>(Parent))
				{
					return Scroll;
				}
				Cur = Parent;
				continue;
			}
			// 위젯 트리의 뿌리면 그 트리를 가진 유저 위젯에서 바깥 트리로 이어 간다
			Cur = Cur->GetTypedOuter<UUserWidget>();
		}
		return nullptr;
	}
}

const FName UMissionManagerSubsystem::OpeningChainStartID = TEXT("M1_CollectBricks");

namespace
{
	APlayerCamera* FindPlayerCamera(UWorld* World)
	{
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		return PC ? Cast<APlayerCamera>(PC->GetPawn()) : nullptr;
	}

	// 배치 프리뷰는 플레이어 폰의 PlacementHandler 가 소유한다 (IsPlacementActive 와 같은 경로)
	AActor* FindPlacementPreviewActor(const UGameInstance* GI)
	{
		UWorld* GuideWorld = GI ? GI->GetWorld() : nullptr;
		APlayerController* PC = GuideWorld ? GuideWorld->GetFirstPlayerController() : nullptr;
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		UPlacementHandler* Handler = Pawn ? Pawn->FindComponentByClass<UPlacementHandler>() : nullptr;
		return Handler ? Handler->GetPlacementTargetActor() : nullptr;
	}

	// 소유 목록의 기본 스킨 — 스킨 매니저의 같은 상수는 cpp 비공개라 여기 둔다
	constexpr int32 TutorialStarterSkinID = 100;

	// 도시 회사 상태 — 도시 맵은 WorldSubsystem, 사무실처럼 매니저가 비어 있는 맵은 저장값으로 본다
	bool HasCityCompanyInState(UWorld* CityWorld, const USaveGame_GameData* Save, EAcqState State)
	{
		const UCityAcquisitionManager* AcqMgr = CityWorld ? CityWorld->GetSubsystem<UCityAcquisitionManager>() : nullptr;
		if (AcqMgr && AcqMgr->FindFirstCompanyKeyInState(State) != INDEX_NONE)
		{
			return true;
		}
		if (Save)
		{
			for (const TPair<int32, FCityAcqSave>& Entry : Save->GameData.CityAcq)
			{
				if (Entry.Value.State == static_cast<uint8>(State))
				{
					return true;
				}
			}
		}
		return false;
	}
}

void UMissionManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// DT 캐시/세이브 로드가 먼저 준비돼야 OnGameDataLoaded 시점에 미션 복원 가능
	Collection.InitializeDependency<UTableManagerSubsystem>();
	Collection.InitializeDependency<USaveLoadManager>();
	Collection.InitializeDependency<UResourceItemManager>();
	// UIManager 선초기화 필수 (아래 OnLevelLayerReady 구독)
	// 누락 시 GetSubsystem null → 구독 스킵 → 트래커 미표시 회귀 있었음
	Collection.InitializeDependency<UUIManagerSubsystem>();
	// M7 OnRevenueCollected 구독용 선초기화 (순서 누락 시 구독 스킵)
	Collection.InitializeDependency<UProjectOperationManager>();
	Collection.InitializeDependency<UMineManager>();
	Collection.InitializeDependency<UWorldFactoryManager>();
	// OnEmployeeEnhanced 구독용 선초기화 (순서 누락 시 구독 스킵)
	Collection.InitializeDependency<UEmployeeManager>();
	Collection.InitializeDependency<URecruitmentManagerSubsystem>();
	Collection.InitializeDependency<UBuildingTraitManagerSubsystem>();
	// 도출형 설명 걸음(M29 본사·M31 메뉴)의 코치마크 완료 신호 구독용 선초기화
	Collection.InitializeDependency<UPanelIntroSubsystem>();

	// 세이브 복원 완료 신호. 진행 중 미션 ID → DT 행 복원 진입점
	if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
	{
		SaveMgr->OnGameDataLoaded.AddUObject(this, &UMissionManagerSubsystem::HandleGameDataLoaded);
	}
	// 레벨 UI 레이어 완료 신호에 트래커·가이드 빌드 연결 (StartPlay next-tick 경쟁 제거)
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
		// 퀘스트 카운터는 명시 수거 전용 신호로만 — OnRevenueCollected 는 틱 적립도 타서 초당 1회 들어온다
		OpMgr->OnStoredRevenueCollected.AddUObject(this, &UMissionManagerSubsystem::HandleStoredRevenueCollected);
	}
	if (UMineManager* MineMgr = GetGameInstance()->GetSubsystem<UMineManager>())
	{
		MineMgr->OnResourceClaimed.AddDynamic(this, &UMissionManagerSubsystem::HandleMineResourceClaimed);
	}
	if (UWorldFactoryManager* FactoryMgr = GetGameInstance()->GetSubsystem<UWorldFactoryManager>())
	{
		FactoryMgr->OnLineClaimed.AddDynamic(this, &UMissionManagerSubsystem::HandleFactoryLineClaimed);
	}
	// 직원 강화 완료 구독 (목표판 G9 판정, 스킵 시 G9 영구 미완)
	if (UEmployeeManager* EmpMgr = GetGameInstance()->GetSubsystem<UEmployeeManager>())
	{
		EmpMgr->OnEmployeeEnhanced.AddUObject(this, &UMissionManagerSubsystem::HandleEmployeeEnhanced);
		EmpMgr->OnEmployeeLevelUp.AddUObject(this, &UMissionManagerSubsystem::HandleEmployeeLevelUp);
		EmpMgr->OnEmployeeStatPointAllocated.AddUObject(this, &UMissionManagerSubsystem::HandleEmployeeStatPointAllocated);
	}
	if (UPanelIntroSubsystem* IntroMgr = GetGameInstance()->GetSubsystem<UPanelIntroSubsystem>())
	{
		IntroMgr->OnIntroCompleted.AddUObject(this, &UMissionManagerSubsystem::HandlePanelIntroCompleted);
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
		OpMgr->OnStoredRevenueCollected.RemoveAll(this);
	}
	if (UMineManager* MineMgr = GetGameInstance()->GetSubsystem<UMineManager>())
	{
		MineMgr->OnResourceClaimed.RemoveDynamic(this, &UMissionManagerSubsystem::HandleMineResourceClaimed);
	}
	if (UWorldFactoryManager* FactoryMgr = GetGameInstance()->GetSubsystem<UWorldFactoryManager>())
	{
		FactoryMgr->OnLineClaimed.RemoveDynamic(this, &UMissionManagerSubsystem::HandleFactoryLineClaimed);
	}
	if (UEmployeeManager* EmpMgr = GetGameInstance()->GetSubsystem<UEmployeeManager>())
	{
		EmpMgr->OnEmployeeEnhanced.RemoveAll(this);
		EmpMgr->OnEmployeeLevelUp.RemoveAll(this);
		EmpMgr->OnEmployeeStatPointAllocated.RemoveAll(this);
	}
	if (UPanelIntroSubsystem* IntroMgr = GetGameInstance()->GetSubsystem<UPanelIntroSubsystem>())
	{
		IntroMgr->OnIntroCompleted.RemoveAll(this);
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

	// 첫 배달 뒤 현재 정원을 채우고 증축한다. 복원도 실제 인원과 정원을 따른다.
	int32 LaborerCount = 0;
	int32 LaborerCapacity = 0;
	int32 ExpansionLevel = 1;
	if (ABrickFactory* Factory = Cast<ABrickFactory>(BrickFactoryWeak.Get()))
	{
		LaborerCount = Factory->GetDesiredLaborerCount();
		LaborerCapacity = Factory->GetLaborerCapacity();
		ExpansionLevel = Factory->GetUpgradeLevel(EFactoryUpgradeType::FactoryExpansion);
	}
	UWorld* CurWorld = GetWorld();
	const ACGGameModeBase* CityMode = CurWorld ? CurWorld->GetAuthGameMode<ACGGameModeBase>() : nullptr;
	const bool bWatchingFirstDelivery = GuidePhase == BrickGuide::ObserveLaborers
		&& ((CityMode && CityMode->IsCityOpeningActive()) || GetPhaseElapsedSeconds() < 2.0);
	const int32 DeliveryPhase = bWatchingFirstDelivery
		? GuidePhase : BrickGuide::PhaseAfterDelivery(GuidePhase, NewValue, LaborerCount, LaborerCapacity, ExpansionLevel);
	if (DeliveryPhase != GuidePhase)
	{
		SetGuidePhase(DeliveryPhase);
	}

	OnMissionProgressChanged.Broadcast();

	if (BrickGuide::CanComplete(GuidePhase, NewValue, ActiveMissionRow.ConditionAmount))
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
	const bool bSavedRevenueReceived = Save && Save->GameData.bCurrentMissionRevenueReceived;
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
	bTutorialRevenueReceived = FTutorialRevenueCompletionRules::RestoreReceipt(
		bSavedRevenueReceived, SavedMissionID, ActiveMissionID,
		HasActiveMission() && (ActiveMissionRow.ConditionType == EMissionConditionType::CollectFirstRevenue
			|| ActiveMissionRow.ConditionType == EMissionConditionType::LaunchFirstInHouseProject));

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

	DerivedMissionLatches = FTutorialDerivedMissionRules::ResolveRestoredLatches(
		Save ? Save->GameData.DerivedMissionLatchID : NAME_None,
		ActiveMissionID,
		IsDerivedGuideActive(),
		Save ? Save->GameData.DerivedMissionLatches : TArray<FName>());
	if (DerivedMissionLatches.Num() > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[MissionManager] 도출형 래치 복원: %s %d개"),
			*ActiveMissionID.ToString(), DerivedMissionLatches.Num());
	}

	if (FTutorialMissionRestoreRules::ShouldRestoreClaimReady(
		bSavedClaimReady,
		SavedMissionID,
		ActiveMissionID,
		HasActiveMission() && ActiveMissionRow.bManualClaim))
	{
		bReadyToClaim = true;
		UE_LOG(LogTemp, Log, TEXT("[MissionManager] 보상 클레임 대기 상태 복원: %s"), *ActiveMissionID.ToString());
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

void UMissionManagerSubsystem::ApplyDevModeCheat()
{
#if !UE_BUILD_SHIPPING
	// 미션을 비우면 트래커/오버레이/상주 알림이 전부 "체인 없음" 분기로 내려간다.
	SetActiveMission(NAME_None);

	// 일반 강화·미션판이 열리는 조건이라 dev 점프 래치는 같이 내린다(래치가 남으면 스킵해도 잠긴 채다).
	bTutorialCompleted = true;
	bDevTutorialJumpSessionActive = false;
	bGuideSuspendedForLaunchReward = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PollTimerHandle);
	}

	// 트래커는 완료 연출로 스스로 숨는 경로를 안 타므로(중도 종료) 여기서 직접 재settle 시킨다.
	if (UMissionTrackerWidget* Tracker = GetActiveTracker())
	{
		Tracker->RefreshFromManager();
	}
	UpdateGuideNotification();

	if (UGoalBoardSubsystem* GoalMgr = GetGameInstance()->GetSubsystem<UGoalBoardSubsystem>())
	{
		GoalMgr->UnlockBoard();
	}

	ApplyDevSandbox(TEXT("Rich"), 20);

	UE_LOG(LogTemp, Warning, TEXT("[MissionManager] DevMode 치트 — 튜토리얼 종료 + 미션판 언락 + 무한모드 적용"));
#endif
}

void UMissionManagerSubsystem::ApplyDevSandbox(FName ScenarioOverride, int32 HQLevelOverride)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 다음 틱으로 미룬다 — 다른 OnGameDataLoaded 핸들러(ResourceItemManager 등)가 세이브를 먼저
	// 메모리에 올린 뒤에 덮어써야 샌드박스 값이 살아남는다 (핸들러 호출 순서 비보장 회피).
	World->GetTimerManager().SetTimerForNextTick([WeakThis = TWeakObjectPtr<UMissionManagerSubsystem>(this), ScenarioOverride, HQLevelOverride]()
	{
		UMissionManagerSubsystem* Self = WeakThis.Get();
		UGameInstance* GI = Self ? Self->GetGameInstance() : nullptr;
		if (!GI)
		{
			return;
		}

		const UCGDevSettings* Dev = GetDefault<UCGDevSettings>();

		// 자원 듬뿍 — 기존 GrantTestResources 재사용 (DT_TestResourceScenarios Row, MarketCap 이 시총 게이트 개방)
		const FName Scenario = !ScenarioOverride.IsNone()
			? ScenarioOverride
			: (Dev ? Dev->SandboxResourceScenario : NAME_None);
		if (!Scenario.IsNone())
		{
			if (UResourceItemManager* Res = GI->GetSubsystem<UResourceItemManager>())
			{
				// 부팅 자동 적용이라 Diamond 는 건너뛴다 ― 서버 권위 재화다(§7.6 R3). 콘솔 `GrantRes` 는 그대로 넣는다
				Res->GrantTestResourcesExceptDiamond(Scenario);
			}
		}

		// 전체 해금 — HQ 레벨 강제 (HQ 게이트 콘텐츠). 세이브엔 안 쓰고 세션 메모리 + UI 브로드캐스트만.
		const int32 TargetHQ = HQLevelOverride > 0 ? HQLevelOverride : (Dev ? Dev->SandboxHQLevel : 20);
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
			*Scenario.ToString(), TargetHQ);
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
			Res->GrantTestResourcesExceptDiamond(TEXT("Rich"));
		}

		// 빌딩 선행: M1/M2(빌딩 만들기 전·중) 외 미션은 지어진 빌딩이 있어야 의미가 있다.
		// 빌딩 자동 스폰은 유효 플롯 확보가 필요해 v1 범위 밖 — 빌딩이 없으면 명확히 안내.
		// (개발 워크플로: 한 번 건설하면 세이브에 남아 이후 M3+ 점프를 빌딩 재건설 없이 반복 가능)
		const bool bNeedsBuilding =
			TargetMissionID != TEXT("M1_CollectBricks") &&
			TargetMissionID != TEXT("M2_UpgradeFactory") &&
			TargetMissionID != TEXT("M3_BuildFirstCompany");
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
		if (TargetMissionID == TEXT("M6_FirstRecruit"))
		{
			if (UItemInventoryManager* ItemMgr = GI->GetSubsystem<UItemInventoryManager>())
			{
				const int32 Owned = ItemMgr->GetItemCount(EItemType::RecruitTicketNormal);
				const int32 RequiredCount = static_cast<int32>(FMath::Clamp<int64>(Self->ActiveMissionRow.ConditionAmount, 1, MAX_int32));
				if (Owned < RequiredCount)
				{
					ItemMgr->AddItem(EItemType::RecruitTicketNormal, RequiredCount - Owned);
					UE_LOG(LogTemp, Log, TEXT("[MissionManager] 시드: 일반 채용권 %d장 지급 (M5 개발자 점프)"), RequiredCount - Owned);
				}
			}
		}

		Self->SeedDerivedJumpPrereqs(TargetMissionID);

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
	const bool bCarryFirstProjectReceipt = bTutorialRevenueReceived
		&& ActiveMissionRow.ConditionType == EMissionConditionType::LaunchFirstInHouseProject;
	if (IsExplainPhaseActive())
	{
		ReleaseExplainPresentation();
	}
	ActiveMissionID = NAME_None;
	ActiveMissionRow = FMissionTable();
	if (URecruitmentManagerSubsystem* RecruitMgr = GetGameInstance()->GetSubsystem<URecruitmentManagerSubsystem>())
	{
		RecruitMgr->SeedForcedPrimaryDisciplines({});
	}
	GuidePhase = 0;
	PhaseEnteredAt = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	TransitionMissCount = 0;
	DeskPlaceProgress = 0;
	bRecruitJobSelected = false;
	bBuildIndustrySelected = false;
	bDerivedBuildIndustryPicked = false;
	bReadyToClaim = false;
	bTutorialRevenueReceived = false;
	FactoryBoostStartedAt = -1.0;
	DerivedMissionLatches.Reset();
	DerivedFloorSignalAt = -1.0;
	DerivedDemolishSignalAt = -1.0;
	DerivedPlotDialogOpenedAt = -1.0;
	DerivedHQLevelSignalAt = -1.0;
	bDerivedEvaluatedOnce = false;
	DerivedIntroPanelWeak.Reset();

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
	bTutorialRevenueReceived = bCarryFirstProjectReceipt
		&& Row.ConditionType == EMissionConditionType::CollectFirstRevenue;

	// 튜토리얼 채용(M5 첫 채용·M14·M24 목표 직능 채용)만 직능을 맞춘다. 미션 복원 경로도 이 함수를 지나므로 저장 없이 idempotent.
	if (ActiveMissionRow.ConditionType == EMissionConditionType::RecruitEmployees
		|| (ActiveMissionRow.ConditionType == EMissionConditionType::RecruitGoalDiscipline && ActiveMissionRow.bDerivedGuide))
	{
		if (URecruitmentManagerSubsystem* RecruitMgr = GetGameInstance()->GetSubsystem<URecruitmentManagerSubsystem>())
		{
			RecruitMgr->SeedForcedPrimaryDisciplines(
				TutorialDisciplineSeed::BuildSeedList(ActiveMissionRow.RequiredRecruitDiscipline,
					static_cast<int32>(FMath::Clamp<int64>(ActiveMissionRow.ConditionAmount, 0, MAX_int32))));
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

	if (const ACGGameModeBase* CityMode = World->GetAuthGameMode<ACGGameModeBase>())
	{
		if (CityMode->IsCityOpeningActive())
		{
			World->GetTimerManager().ClearTimer(PollTimerHandle);
			return;
		}
	}

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

	// 공장 액터는 실제 홀드 제스처와 재진입 시 인부 레벨 조회에 사용한다.
	if (bMainMap)
	{
		if (ABrickFactory* Factory = Cast<ABrickFactory>(
			UGameplayStatics::GetActorOfClass(World, ABrickFactory::StaticClass())))
		{
			BrickFactoryWeak = Factory;
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
		if (!TutorialFirstBuildingWeak.IsValid())
		{
			TutorialFirstBuildingWeak = PickGuideBuilding();
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
			bCanExplain ? TEXT("Explain") : TEXT("WatchIncome(설명 건너뜀)"));
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

		SetGuidePhase(bCanExplain ? CollectGuide::Explain : CollectGuide::WatchIncome);
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
	// 모달은 열 때마다 첫 산업을 자동 선택한다 — 산업 타일 걸음은 여는 회마다 다시 보여 준다
	if (IsDerivedGuideActive())
	{
		bDerivedBuildIndustryPicked = false;
		return;
	}

	if (ActiveMissionRow.ConditionType == EMissionConditionType::BuildFirstBuilding)
	{
		BuildModalWeak = Modal;
		SetGuidePhase(bBuildIndustrySelected ? BuildGuide::PickBuilding : BuildGuide::PickIndustry);
	}
}

void UMissionManagerSubsystem::NotifyBuildIndustrySelected(UBuildModalWidget* Modal, ECompanyType Industry)
{
	if (IsDerivedGuideActive() && (ActiveMissionRow.ConditionType == EMissionConditionType::BuildSecondBuilding
		|| ActiveMissionRow.ConditionType == EMissionConditionType::BuildIndustry)
		&& Modal && Modal->IsActivated() && Industry == MakeActiveMissionSpec().Industry)
	{
		bDerivedBuildIndustryPicked = true;
		return;
	}
	if (HasActiveMission() && ActiveMissionRow.ConditionType == EMissionConditionType::BuildFirstBuilding
		&& GuidePhase == BuildGuide::PickIndustry && Modal && BuildModalWeak.Get() == Modal
		&& Modal->IsActivated() && Industry == ECompanyType::Game
		&& Modal->GetBuildGuideIndustryWidget())
	{
		bBuildIndustrySelected = true;
		SetGuidePhase(BuildGuide::PickBuilding);
	}
}

void UMissionManagerSubsystem::RegisterFactoryPanel(UFactoryPanelWidget* Panel)
{
	FactoryPanelWeak = Panel;

	// 공장 패널 열림 = OpenFactory 페이즈 통과 → 강화 유도로 전환
	if (HasActiveMission() && ActiveMissionRow.ConditionType == EMissionConditionType::CollectBricks
		&& GuidePhase == BrickGuide::OpenFactory)
	{
		SetGuidePhase(BrickGuide::AddLaborer);
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

void UMissionManagerSubsystem::NotifyFactoryUpgradePurchased(EFactoryUpgradeType UpgradeType, int32 PurchasedLevels)
{
	if (PurchasedLevels > 0)
	{
		OnConditionSignal.Broadcast(EMissionConditionType::UpgradeFactoryStats, PurchasedLevels);
		if (UpgradeType == EFactoryUpgradeType::HoldProductionAmount)
		{
			OnConditionSignal.Broadcast(EMissionConditionType::ReachFactoryProductionLevel, 1);
		}
	}
	if (IsDerivedGuideActive())
	{
		EvaluateDerivedMissionCompletion();
		return;
	}
	if (!HasActiveMission() || ActiveMissionRow.ConditionType != EMissionConditionType::CollectBricks)
	{
		return;
	}

	if (!BrickFactoryWeak.IsValid())
	{
		BrickFactoryWeak = UGameplayStatics::GetActorOfClass(GetWorld(), ABrickFactory::StaticClass());
	}
	const ABrickFactory* Factory = Cast<ABrickFactory>(BrickFactoryWeak.Get());
	const int32 PurchasePhase = BrickGuide::PhaseAfterPurchase(GuidePhase, UpgradeType,
		Factory ? Factory->GetDesiredLaborerCount() : 0,
		Factory ? Factory->GetLaborerCapacity() : 0);
	if (PurchasePhase != GuidePhase)
	{
		SetGuidePhase(PurchasePhase);
	}
}

void UMissionManagerSubsystem::NotifyBuildingStatUpgraded(int32 PurchasedLevels)
{
	if (PurchasedLevels > 0)
	{
		OnConditionSignal.Broadcast(EMissionConditionType::EnhanceBuildingStat, PurchasedLevels);
	}
}

void UMissionManagerSubsystem::NotifyManufacturedProductSold(int64 QuantitySold)
{
	if (QuantitySold > 0)
	{
		OnConditionSignal.Broadcast(EMissionConditionType::SellManufacturedProduct, QuantitySold);
	}
}

void UMissionManagerSubsystem::NotifyReserveManufacturingProduct(int32 ReservedQuantity)
{
	if (ReservedQuantity > 0)
	{
		OnConditionSignal.Broadcast(EMissionConditionType::ReserveManufacturingProduct, ReservedQuantity);
	}
}

void UMissionManagerSubsystem::NotifyFactoryBoostStarted()
{
	if (!HasActiveMission() || ActiveMissionRow.ConditionType != EMissionConditionType::CollectBricks)
	{
		return;
	}
	if (GuidePhase == BrickGuide::BoostLaborers)
	{
		FactoryBoostStartedAt = GetWorld() ? GetWorld()->GetTimeSeconds() : -1.0;
	}
}

void UMissionManagerSubsystem::SetReadyToClaim()
{
	if (bReadyToClaim || !HasActiveMission())
	{
		return;
	}

	// 명시적으로 수령이 필요한 미션만 클레임 대기로 전환한다.
	const ETutorialMissionCompletionMode CompletionMode = FTutorialMissionCompletionRules::Resolve(
		ActiveMissionRow.bManualClaim);
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

	// 가이드 전부 해제 — 조회 게터는 클레임 대기 중 null, 상주 알림도 여기서 해제
	UpdateGuideNotification();
	OnMissionReadyToClaim.Broadcast(ActiveMissionRow);

	UE_LOG(LogTemp, Log, TEXT("[MissionManager] 보상 클레임 대기 진입: %s"), *ActiveMissionID.ToString());
}

void UMissionManagerSubsystem::SetRecruitmentPresentationActive(bool bActive)
{
	if (bRecruitmentPresentationActive == bActive) { return; }
	bRecruitmentPresentationActive = bActive;
	RefreshTrackerPresentationSuppression();
	if (GuideOverlayWidget)
	{
		if (bActive)
		{
			GuideOverlayVisibilityBeforeRecruitment = GuideOverlayWidget->GetVisibility();
			bGuideOverlayVisibilitySavedForRecruitment = true;
			GuideOverlayWidget->SetVisibility(ESlateVisibility::Hidden);
		}
		else if (bGuideOverlayVisibilitySavedForRecruitment)
		{
			GuideOverlayWidget->SetVisibility(GuideOverlayVisibilityBeforeRecruitment);
		}
	}
	if (!bActive) { bGuideOverlayVisibilitySavedForRecruitment = false; }
}

bool UMissionManagerSubsystem::IsClaimPresentable() const
{
	if (bRecruitmentPresentationActive) { return false; }
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
				RewardSplashWeak = Overlay;
				Overlay->OnPresentationClosed.AddUObject(this, &UMissionManagerSubsystem::HandleRewardSplashClosed, TWeakObjectPtr<UUserWidget>(Overlay));
				// AddToViewport(NativeConstruct: BindWidget 바인드 + 버튼 OnClicked) 먼저 → 그 다음 박스 구성
				Overlay->AddToViewport(10000); // 가이드(9000)/토스트(9999) 위
				Overlay->SetupRewards(Mission.Rewards, SplashTitle, /*bToastMode=*/false);
				RefreshTrackerPresentationSuppression();
				return;
			}
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("[MissionManager] 최종 보상 연출 위젯 미등록 — 경량 폴백"));

	// 폴백 = 경량 토스트 (팝인→홀드→페이드아웃 후 스스로 RemoveFromParent)
	URewardClaimSplashWidget* Splash = CreateWidget<URewardClaimSplashWidget>(PC, URewardClaimSplashWidget::StaticClass());
	if (Splash)
	{
		RewardSplashWeak = Splash;
		Splash->InitSplash(Mission);
		Splash->AddToViewport(9500); // 가이드 오버레이(9000) 위 / 토스트(9999) 아래
	}
}

bool UMissionManagerSubsystem::IsRewardSplashShown() const
{
	const UUserWidget* Splash = RewardSplashWeak.Get();
	return Splash && Splash->IsInViewport();
}

bool UMissionManagerSubsystem::IsRewardSplashRevealComplete() const
{
	const URewardRevealPresentationWidget* Reveal = Cast<URewardRevealPresentationWidget>(RewardSplashWeak.Get());
	return !IsRewardSplashShown() || !Reveal || Reveal->IsRevealComplete();
}

void UMissionManagerSubsystem::HandleRewardSplashClosed(TWeakObjectPtr<UUserWidget> ClosedSplash)
{
	if (RewardSplashWeak == ClosedSplash)
	{
		RewardSplashWeak.Reset();
	}
	RefreshTrackerPresentationSuppression();
	OnRewardSplashClosed.Broadcast();
}

void UMissionManagerSubsystem::RefreshTrackerPresentationSuppression()
{
	if (UMissionTrackerWidget* ActiveTracker = GetActiveTracker()) { ActiveTracker->SetPresentationSuppressed(bRecruitmentPresentationActive); }
	if (GoalTrackerWidget) { GoalTrackerWidget->SetPresentationSuppressed(ShouldHideGoalTracker()); }
}

bool UMissionManagerSubsystem::ShouldHideGoalTracker() const
{
	if (bRecruitmentPresentationActive || IsRewardSplashShown())
	{
		return true;
	}
	const UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
	const UUIBase* UIBase = UIMgr ? UIMgr->GetUIBase() : nullptr;
	return UIBase && UIBase->GetPromptStackCount() > 0;
}

// 배치 완료 직후 현장 호출 훅. 조건 판정은 매니저 한 곳에
void UMissionManagerSubsystem::NotifyBuildingPlaced(ABuildingBaseActor* Building)
{
	// 건설 채수 도달형 미션 신호 — 판정(보유 채수 절대값)은 GoalBoard 가 수행
	OnConditionSignal.Broadcast(EMissionConditionType::BuildSecondBuilding, 1);
	OnConditionSignal.Broadcast(EMissionConditionType::BuildOnClearedPlot, 1);
	OnConditionSignal.Broadcast(EMissionConditionType::BuildIndustry, 1);

	if (!HasActiveMission())
	{
		return;
	}

	if (ActiveMissionRow.ConditionType == EMissionConditionType::BuildFirstBuilding)
	{
		// 방금 지은 빌딩을 다음 미션(M3)의 스포트라이트 대상으로 캡처
		// GuidePhase와 별개 멤버라 M2→M3 전환에도 유지
		TutorialFirstBuildingWeak = Building;

		SetReadyToClaim();
		return;
	}
	EvaluateDerivedMissionCompletion();
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
		ResumeGuideAfterLaunchRewardConfirmed();
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

void UMissionManagerSubsystem::NotifyOfficeRecruitmentModalOpened(UOfficeRecruitmentModalWidget* Modal)
{
	RecruitModalWeak = Modal;
	GoalRecruitClickedDiscipline = EProductionDiscipline::Count;
	if (HasActiveMission() && ActiveMissionRow.ConditionType == EMissionConditionType::RecruitEmployees
		&& GuidePhase == RecruitGuide::OpenRecruit)
	{
		SetGuidePhase(bRecruitJobSelected ? RecruitGuide::PressPull : RecruitGuide::PickJob);
	}
}

void UMissionManagerSubsystem::NotifyRecruitJobSelected(UOfficeRecruitmentModalWidget* Modal, EProductionDiscipline Discipline)
{
	if (Modal && RecruitModalWeak.Get() == Modal)
	{
		GoalRecruitClickedDiscipline = Discipline;
	}
	if (HasActiveMission() && ActiveMissionRow.ConditionType == EMissionConditionType::RecruitEmployees
		&& Modal && RecruitModalWeak.Get() == Modal && GuidePhase == RecruitGuide::PickJob
		&& (GetRequiredRecruitDiscipline() == EProductionDiscipline::Count || GetRequiredRecruitDiscipline() == Discipline))
	{
		bRecruitJobSelected = true;
		SetGuidePhase(RecruitGuide::PressPull);
	}
}

void UMissionManagerSubsystem::RegisterGachaPresentation(UEmployeeGachaPresentationWidget* Widget)
{
	GachaPresentationWeak = Widget;
	if (HasActiveMission() && ActiveMissionRow.ConditionType == EMissionConditionType::RecruitEmployees
		&& GuidePhase <= RecruitGuide::PressPull)
	{
		SetGuidePhase(RecruitGuide::PressConfirm);
	}
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

void UMissionManagerSubsystem::ReportQuestCounter(EQuestCounter Counter, int64 Amount)
{
	// 퀘스트 원장은 미션판/튜토리얼과 결합하지 않는다 — 여기는 기존 팬인 지점을 빌려 신호만 흘린다
	if (UQuestSubsystem* QuestMgr = GetGameInstance()->GetSubsystem<UQuestSubsystem>())
	{
		QuestMgr->ReportCounter(Counter, Amount);
	}
}

void UMissionManagerSubsystem::NotifyHireConfirmed(const FGachaResultData& /*Result*/)
{
	ReportQuestCounter(EQuestCounter::RecruitHire);
	OnConditionSignal.Broadcast(EMissionConditionType::RecruitGoalDiscipline, 1);
	// 확인 뒤 목표에 도달했을 때만 패널 닫기를 유도한다. ×1 첫 채용은 다시 뽑기로 돌아간다.
	if (HasActiveMission()
		&& ActiveMissionRow.ConditionType == EMissionConditionType::RecruitEmployees
		&& GuidePhase <= RecruitGuide::PressConfirm)
	{
		int64 Current = 0;
		int64 Target = 0;
		const bool bReachedTarget = GetMissionProgress(Current, Target)
			&& FRecruitEmployeesMissionRules::HasReachedTarget(Current, Target);
		SetGuidePhase(bReachedTarget ? RecruitGuide::PressCloseRecruit : RecruitGuide::PressPull);
	}

	EvaluateRecruitEmployeesCondition();
	EvaluateDerivedMissionCompletion();
}

void UMissionManagerSubsystem::NotifyRecruitPanelClosed()
{
	RecruitModalWeak.Reset();
	GoalRecruitClickedDiscipline = EProductionDiscipline::Count;
	if (HasActiveMission() && ActiveMissionRow.ConditionType == EMissionConditionType::RecruitEmployees)
	{
		int64 Current = 0;
		int64 Target = 0;
		if (GetMissionProgress(Current, Target)
			&& !FRecruitEmployeesMissionRules::HasReachedTarget(Current, Target))
		{
			SetGuidePhase(RecruitGuide::OpenRecruit);
		}
	}
	// 패널을 닫은 뒤 절대 직원 수로 완료를 판정한다.
	EvaluateRecruitEmployeesCondition();
}

// ===== M5 좌석 배정 =====
void UMissionManagerSubsystem::NotifyWorkstationPanelOpened(UWorkstationInfoWidget* Panel)
{
	WorkstationPanelWeak = Panel;
}

void UMissionManagerSubsystem::NotifyWorkstationPanelClosed(UWorkstationInfoWidget* Panel)
{
	if (WorkstationPanelWeak.Get() == Panel)
	{
		WorkstationPanelWeak.Reset();
	}
}

void UMissionManagerSubsystem::NotifyWorkstationGrowthChanged(bool bPromoted, int32 PurchasedLevels)
{
	if (bPromoted)
	{
		OnConditionSignal.Broadcast(EMissionConditionType::PromoteFirstWorkstation, 1);
	}
	else if (PurchasedLevels > 0)
	{
		OnConditionSignal.Broadcast(EMissionConditionType::UpgradeFirstWorkstation, PurchasedLevels);
	}
	EvaluateDerivedMissionCompletion();
}

void UMissionManagerSubsystem::NotifyEmployeeSeated()
{
	EvaluateRecruitEmployeesCondition();
	OnConditionSignal.Broadcast(EMissionConditionType::RecruitGoalDiscipline, 1);
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
	if (HasActiveMission()
		&& ActiveMissionRow.ConditionType == EMissionConditionType::LaunchFirstInHouseProject
		&& GuidePhase == InHouseGuide::WatchStrip)
	{
		const UOfficeStageProgressManager* StageMgr = GetWorld() ? GetWorld()->GetSubsystem<UOfficeStageProgressManager>() : nullptr;
		if (StageMgr)
		{
			// 미달 창은 [출시] 대신 [추가 개발]/[폐기] ― 다이아가 모자란 튜토리얼에서는 폐기 후 다시 착수가 길이다
			SetGuidePhase(StageMgr->IsLaunchBlocked() ? InHouseGuide::DiscardFailed : InHouseGuide::PressLaunch);
		}
	}
}

void UMissionManagerSubsystem::UnregisterLaunchConfirm(ULaunchConfirmWidget* Widget)
{
	if (LaunchConfirmWeak.Get() == Widget)
	{
		LaunchConfirmWeak.Reset();
		if (HasActiveMission()
			&& ActiveMissionRow.ConditionType == EMissionConditionType::LaunchFirstInHouseProject
			&& (GuidePhase == InHouseGuide::WatchStrip || GuidePhase == InHouseGuide::PressLaunch || GuidePhase == InHouseGuide::DiscardFailed))
		{
			const UOfficeStageProgressManager* StageMgr = GetWorld() ? GetWorld()->GetSubsystem<UOfficeStageProgressManager>() : nullptr;
			if (StageMgr && StageMgr->GetLifecycle() == EProjectLifecycle::Idle)
			{
				SetGuidePhase(InHouseGuide::OpenBoard);
			}
		}
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
	}
}

void UMissionManagerSubsystem::NotifyProjectLaunched()
{
	ReportQuestCounter(EQuestCounter::ProjectLaunch);
	OnConditionSignal.Broadcast(EMissionConditionType::LaunchGoalProject, 1);
	// >= WatchStrip — 개발 완료와 출시 확인창 전환 사이에 출시 신호가 와도 완료를 놓치지 않는다.
	if (HasActiveMission()
		&& ActiveMissionRow.ConditionType == EMissionConditionType::LaunchFirstInHouseProject
		&& GuidePhase >= InHouseGuide::WatchStrip)
	{
		// 운영은 백그라운드로 시작한다. 다음 단계 안내는 출시 보상 리빌 확인 뒤에 표시한다.
		SetReadyToClaim();
		return;
	}
	EvaluateDerivedMissionCompletion();
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
	// 지급 사실을 먼저 보존하고, 완료 전이는 설명과 수익 관찰이 끝난 뒤 폴에서 처리한다.
	if (HasActiveMission()
		&& (ActiveMissionRow.ConditionType == EMissionConditionType::CollectFirstRevenue
			|| (ActiveMissionRow.ConditionType == EMissionConditionType::LaunchFirstInHouseProject && HasLaunchedRequiredProject()))
		&& FTutorialRevenueCompletionRules::IsPositiveCollection(Amount) && !bTutorialRevenueReceived)
	{
		bTutorialRevenueReceived = true;
		if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
		{
			SaveMgr->RequestDeferredSave();
		}
	}
}

void UMissionManagerSubsystem::HandleStoredRevenueCollected(int64 Amount)
{
	// "수익 N회 수거" 원장은 명시 수거 1회 = 1 이어야 한다. 초당 적립이 여기로 들어오면 데일리/위클리가 자동 완성된다.
	if (Amount > 0) ReportQuestCounter(EQuestCounter::RevenueCollect);
}

void UMissionManagerSubsystem::HandleMineResourceClaimed(ECountryType /*Country*/, EResourceType /*Resource*/, int64 ClaimedQty)
{
	if (ClaimedQty > 0)
	{
		OnConditionSignal.Broadcast(EMissionConditionType::CollectMineResource, ClaimedQty);
	}
}

void UMissionManagerSubsystem::HandleFactoryLineClaimed(ECountryType /*Country*/, int32 /*LineId*/, int64 FinalQty)
{
	if (FinalQty > 0)
	{
		OnConditionSignal.Broadcast(EMissionConditionType::CollectFactoryProduct, FinalQty);
	}
}

// ===== 미션판 G10 EquipTrait =====
void UMissionManagerSubsystem::NotifyTraitEquipped()
{
	ReportQuestCounter(EQuestCounter::TraitEquip);
	// 특성 장착은 미션판 G10 조건 — 판정은 GoalBoard 가 한다 (미션 유무와 무관하게 신호를 흘린다)
	OnConditionSignal.Broadcast(EMissionConditionType::EquipFirstTrait, 1);
	EvaluateDerivedMissionCompletion();
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
	ReportQuestCounter(EQuestCounter::SkinEquip);
	// 스킨 장착은 미션판 G8 조건 — 판정은 GoalBoard 가 한다 (미션 유무와 무관하게 신호를 흘린다)
	OnConditionSignal.Broadcast(EMissionConditionType::EquipFirstSkin, 1);
	if (IsDerivedGuideActive() && ActiveMissionRow.ConditionType == EMissionConditionType::EquipFirstSkin)
	{
		AddDerivedLatch(TutorialDerivedLatch::SkinApplied);
	}
	EvaluateDerivedMissionCompletion();
}

void UMissionManagerSubsystem::NotifyBuildingFloorUpgraded()
{
	ReportQuestCounter(EQuestCounter::FloorRaise);
	OnConditionSignal.Broadcast(EMissionConditionType::RaiseBuildingFloor, 1);
	// 층이 올라가는 모습을 보여 줄 시간을 재는 기준점 — 완료는 연출 뒤로 미룬다(M9 층 상승 관찰)
	if (IsDerivedGuideActive() && ActiveMissionRow.ConditionType == EMissionConditionType::RaiseBuildingFloor)
	{
		DerivedFloorSignalAt = GetWorld() ? GetWorld()->GetTimeSeconds() : -1.0;
	}
	EvaluateDerivedMissionCompletion();
}

// ===== 미션판 G11 AcquireFirstPlot =====
void UMissionManagerSubsystem::NotifyPlotAcquired()
{
	ReportQuestCounter(EQuestCounter::PlotAcquire);
	OnConditionSignal.Broadcast(EMissionConditionType::AcquireFirstPlot, 1);
}

// ===== 미션판 G2 HQLevel =====
void UMissionManagerSubsystem::NotifyHQLevelChanged()
{
	OnConditionSignal.Broadcast(EMissionConditionType::ReachHQLevel, 1);
	// 축하 연출을 보여 줄 시간을 재는 기준점 — 완료는 연출 뒤로 미룬다(M29 레벨업 관찰)
	if (IsDerivedGuideActive() && ActiveMissionRow.ConditionType == EMissionConditionType::ReachHQLevel
		&& DerivedHQLevelSignalAt < 0.0)
	{
		DerivedHQLevelSignalAt = GetWorld() ? GetWorld()->GetTimeSeconds() : -1.0;
	}
	EvaluateDerivedMissionCompletion();
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
	ReportQuestCounter(EQuestCounter::CompanyAcquire);
	OnConditionSignal.Broadcast(EMissionConditionType::AcquireFirstCompany, 1);
	EvaluateDerivedMissionCompletion();
}

void UMissionManagerSubsystem::HandleCompanyCleared(int32 /*Key*/)
{
	OnConditionSignal.Broadcast(EMissionConditionType::DemolishFirstCompany, 1);
	// 붕괴 연출을 다 보기 전에 다음 미션으로 넘어가지 않게 기준 시각을 남긴다
	if (IsDerivedGuideActive() && ActiveMissionRow.ConditionType == EMissionConditionType::DemolishFirstCompany)
	{
		DerivedDemolishSignalAt = GetWorld() ? GetWorld()->GetTimeSeconds() : -1.0;
	}
	EvaluateDerivedMissionCompletion();
}

void UMissionManagerSubsystem::HandleEmployeeEnhanced(int32 /*EmployeeID*/, UEmployeeManager::EEnhanceResult /*Result*/)
{
	ReportQuestCounter(EQuestCounter::EnhanceAttempt);
	OnConditionSignal.Broadcast(EMissionConditionType::EnhanceFirstEmployee, 1);
	// 확률형이라 결과와 무관하게 시도 1회를 인정한다
	if (IsDerivedGuideActive() && ActiveMissionRow.ConditionType == EMissionConditionType::EnhanceFirstEmployee)
	{
		AddDerivedLatch(TutorialDerivedLatch::StarEnhanceAttempted);
	}
	EvaluateDerivedMissionCompletion();
}

void UMissionManagerSubsystem::HandleEmployeeLevelUp(int32 /*EmployeeID*/, int32 /*NewLevel*/)
{
	OnConditionSignal.Broadcast(EMissionConditionType::LevelUpFirstEmployee, 1);
	EvaluateDerivedMissionCompletion();
}

void UMissionManagerSubsystem::HandleEmployeeStatPointAllocated(int32 /*EmployeeID*/)
{
	OnConditionSignal.Broadcast(EMissionConditionType::AllocateEmployeeStatPoint, 1);
	if (IsDerivedGuideActive() && ActiveMissionRow.ConditionType == EMissionConditionType::AllocateEmployeeStatPoint)
	{
		AddDerivedLatch(TutorialDerivedLatch::StatPointAllocated);
	}
	EvaluateDerivedMissionCompletion();
}

// ===== M4 PlaceDesks =====
void UMissionManagerSubsystem::NotifyPlacementModeOpened(EOfficePlacementKind Kind)
{
	if (!HasActiveMission()) return;
	if (Kind == EOfficePlacementKind::Desk
		&& ActiveMissionRow.ConditionType == EMissionConditionType::PlaceDesks
		&& GuidePhase == DeskGuide::OpenPlacement)
		SetGuidePhase(DeskGuide::PickDesk);
}

void UMissionManagerSubsystem::SuspendGuideUntilLaunchRewardConfirmed()
{
	if (!HasActiveMission() || ActiveMissionRow.ConditionType != EMissionConditionType::LaunchFirstInHouseProject)
	{
		return;
	}
	bGuideSuspendedForLaunchReward = true;
	if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
	{
		UIMgr->ClearGuideNotification();
	}
}

void UMissionManagerSubsystem::ResumeGuideAfterLaunchRewardConfirmed()
{
	if (!bGuideSuspendedForLaunchReward)
	{
		return;
	}
	bGuideSuspendedForLaunchReward = false;
	UpdateGuideNotification();
}

void UMissionManagerSubsystem::NotifyDeskSelected()
{
	if (HasActiveMission()
		&& ActiveMissionRow.ConditionType == EMissionConditionType::PlaceDesks
		&& (GuidePhase == DeskGuide::PickDesk || GuidePhase == DeskGuide::OpenPlacement)
		&& IsDeskPlacementActive())
	{
		SetGuidePhase(DeskGuide::Confirm);
	}
}

void UMissionManagerSubsystem::NotifyPlacementCatalogClosed()
{
	if (HasActiveMission()
		&& ActiveMissionRow.ConditionType == EMissionConditionType::PlaceDesks
		&& GuidePhase == DeskGuide::PickDesk
		&& !IsDeskPlacementActive())
	{
		SetGuidePhase(DeskGuide::OpenPlacement);
	}
}

void UMissionManagerSubsystem::NotifyOfficePlacementCompleted(EOfficePlacementKind Kind, int32 Count)
{
	// 그림(벽장식) 배치는 미션판 G7 조건 — 미션 유무와 무관하게 신호를 흘린다
	if (Kind == EOfficePlacementKind::WallDecoration)
	{
		OnConditionSignal.Broadcast(EMissionConditionType::PlaceFirstPainting, 1);
	}

	if (!HasActiveMission()) return;
	// M13 좌석은 저장 상태로 다시 세므로 카운터를 쓰지 않는다. 그림은 스타터 소품과 구분할 수 없어 신호로 래치한다
	if (IsDerivedGuideActive())
	{
		if (Kind == EOfficePlacementKind::WallDecoration
			&& ActiveMissionRow.ConditionType == EMissionConditionType::PlaceFirstPainting)
		{
			AddDerivedLatch(TutorialDerivedLatch::PaintingPlaced);
		}
		EvaluateDerivedMissionCompletion();
		return;
	}
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

void UMissionManagerSubsystem::NotifyOfficeEmployeePanelOpened(UOfficeEmployeePanelWidget* Panel)
{
	OfficeEmployeePanelWeak = Panel;
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
	const bool bRequiresDurableCommit = CompletedRow.bManualClaim || bIsTerminalMission;

	UResourceItemManager* ResMgr = GetGameInstance()->GetSubsystem<UResourceItemManager>();
	UItemInventoryManager* ItemMgr = GetGameInstance()->GetSubsystem<UItemInventoryManager>();
	UGoalBoardSubsystem* GoalMgr = GetGameInstance()->GetSubsystem<UGoalBoardSubsystem>();
	USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>();
	if ((bIsTerminalMission && !GoalMgr)
		|| (CompletedRow.bManualClaim && CompletedRow.Rewards.IsEmpty())
		|| (bRequiresDurableCommit && !FTutorialMissionAtomicCommitRules::AreRewardsGrantable(
			CompletedRow.Rewards, ResMgr != nullptr, ItemMgr != nullptr)))
	{
		UE_LOG(LogTemp, Error,
			TEXT("[MissionManager] 미션 완료 거부 — 보상 관리자 또는 GoalBoard 없음: %s"),
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
	const bool bRevenueReceivedBeforeCommit = bTutorialRevenueReceived;
	const TArray<FName> DerivedLatchesBeforeCommit = DerivedMissionLatches;

	bCompletingMission = true;
	bReadyToClaim = false;
	const FTutorialRewardBroadcastDecision PrepareBroadcastDecision =
		FTutorialMissionAtomicCommitRules::ResolveRewardBroadcastDecision(
			bRequiresDurableCommit,
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
		FTutorialMissionAtomicCommitRules::ResolveSaveDecision(bRequiresDurableCommit, bSaveSucceeded);
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
		bTutorialRevenueReceived = bRevenueReceivedBeforeCommit;
		DerivedMissionLatches = DerivedLatchesBeforeCommit;
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
	LastMissionAdvanceFrame = GFrameCounter;

	const FTutorialRewardBroadcastDecision CommitBroadcastDecision =
		FTutorialMissionAtomicCommitRules::ResolveRewardBroadcastDecision(
			bRequiresDurableCommit,
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
	if (IsDerivedGuideActive())
	{
		EvaluateDerivedMissionCompletion();
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
	if (!ResMgr)
	{
		return;
	}
	const int64 Bricks = ResMgr->GetResourceAmount(EResourceType::Brick);
	const ABrickFactory* Factory = Cast<ABrickFactory>(BrickFactoryWeak.Get());
	const int32 LaborerCount = Factory ? Factory->GetDesiredLaborerCount() : 0;
	const int32 LaborerCapacity = Factory ? Factory->GetLaborerCapacity() : 0;
	const int32 ExpansionLevel = Factory ? Factory->GetUpgradeLevel(EFactoryUpgradeType::FactoryExpansion) : 1;
	UWorld* CurWorld = GetWorld();
	const ACGGameModeBase* CityMode = CurWorld ? CurWorld->GetAuthGameMode<ACGGameModeBase>() : nullptr;
	const bool bWatchingFirstDelivery = GuidePhase == BrickGuide::ObserveLaborers
		&& ((CityMode && CityMode->IsCityOpeningActive()) || GetPhaseElapsedSeconds() < 2.0);
	const int32 DeliveryPhase = bWatchingFirstDelivery
		? GuidePhase : BrickGuide::PhaseAfterDelivery(GuidePhase, Bricks, LaborerCount, LaborerCapacity, ExpansionLevel);
	if (DeliveryPhase != GuidePhase)
	{
		SetGuidePhase(DeliveryPhase);
	}
	if (BrickGuide::CanComplete(GuidePhase, Bricks, ActiveMissionRow.ConditionAmount))
	{
		// 수령 여부는 미션 행의 bManualClaim 정책으로 결정한다.
		SetReadyToClaim();
	}
}

void UMissionManagerSubsystem::EvaluateRecruitEmployeesCondition()
{
	if (!HasActiveMission() || ActiveMissionRow.ConditionType != EMissionConditionType::RecruitEmployees)
	{
		return;
	}
	const UOfficeRecruitmentFlowSubsystem* Flow = GetWorld() ? GetWorld()->GetSubsystem<UOfficeRecruitmentFlowSubsystem>() : nullptr;
	if (Flow && Flow->IsBusy())
	{
		OnMissionProgressChanged.Broadcast();
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
		if (RecruitModalWeak.IsValid())
		{
			// 리빌 확인 전에는 [확인]을 유지하고, 확인 뒤 패널 [닫기]를 안내한다.
			if (GuidePhase != RecruitGuide::PressConfirm)
			{
				SetGuidePhase(RecruitGuide::PressCloseRecruit);
			}
			OnMissionProgressChanged.Broadcast();
		}
		else
		{
			SetReadyToClaim();
		}
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
	FactoryBoostStartedAt = -1.0;
	// 증축 관찰과 저장된 강화 복원의 Hold 안내에서는 열린 패널을 닫아 공장 외형을 보여준다.
	if (ActiveMissionRow.ConditionType == EMissionConditionType::CollectBricks
		&& (NewPhase == BrickGuide::WatchExpansion || NewPhase == BrickGuide::BoostLaborers))
	{
		if (UFactoryPanelWidget* Panel = FactoryPanelWeak.Get())
		{
			Panel->RequestClose();
		}
	}
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
	if (const ACGGameModeBase* CityMode = GetWorld() ? GetWorld()->GetAuthGameMode<ACGGameModeBase>() : nullptr)
	{
		if (CityMode->IsCityOpeningActive()) return;
	}
	if (bGuideSuspendedForLaunchReward)
	{
		return;
	}
	UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
	if (!UIMgr)
	{
		return;
	}

	// 목표는 트래커, 조작 설명은 현재 대상 옆 말풍선이 담당한다.
	UIMgr->ClearGuideNotification();
}

void UMissionManagerSubsystem::PollGuideProgress()
{
	// 상주 알림 리프레시 — 스포트라이트 대상(공장) 캐시가 첫 틱에 늦게 잡히는 타이밍 흡수 (멱등)
	UpdateGuideNotification();
	// 도출형(M8~M31)은 아래 M1~M7 페이즈 보정과 섞지 않는다 — 걸음은 매번 상태에서 다시 구한다
	if (IsDerivedGuideActive())
	{
		if (!bReadyToClaim)
		{
			PollDerivedGuide();
		}
		return;
	}
	if (HasActiveMission() && !bReadyToClaim && !bGuideSuspendedForLaunchReward
		&& ActiveMissionRow.ConditionType == EMissionConditionType::CollectFirstRevenue
		&& GuidePhase == CollectGuide::ExitOffice && GetWorld()->GetMapName().Contains(TEXT("MainMap")))
	{
		TryStartMissionChain();
		return;
	}
	if (HasActiveMission() && ActiveMissionRow.ConditionType == EMissionConditionType::CollectBricks
		&& GuidePhase == BrickGuide::ObserveLaborers)
	{
		if (const ACGGameModeBase* CityMode = GetWorld()->GetAuthGameMode<ACGGameModeBase>())
		{
			if (CityMode->IsCityOpeningActive())
			{
				PhaseEnteredAt = GetWorld()->GetTimeSeconds();
			}
		}
	}
	if (HasActiveMission() && ActiveMissionRow.ConditionType == EMissionConditionType::CollectBricks
		&& GuidePhase <= BrickGuide::ExpandFactory)
	{
		if (!BrickFactoryWeak.IsValid())
		{
			BrickFactoryWeak = UGameplayStatics::GetActorOfClass(GetWorld(), ABrickFactory::StaticClass());
		}
		TryCompleteByResources();
	}
	if (HasActiveMission() && ActiveMissionRow.ConditionType == EMissionConditionType::CollectBricks)
	{
		const int32 WatchPhase = BrickGuide::PhaseAfterExpansionWatch(GuidePhase, GetPhaseElapsedSeconds());
		if (WatchPhase != GuidePhase) SetGuidePhase(WatchPhase);
		if (GuidePhase == BrickGuide::BoostLaborers)
		{
			const ABrickFactory* Factory = Cast<ABrickFactory>(BrickFactoryWeak.Get());
			if (!Factory || !Factory->IsHoldingBoost()) FactoryBoostStartedAt = -1.0;
			else if (FactoryBoostStartedAt < 0.0) FactoryBoostStartedAt = GetWorld()->GetTimeSeconds();
			const double HeldSeconds = FactoryBoostStartedAt >= 0.0
				? GetWorld()->GetTimeSeconds() - FactoryBoostStartedAt : 0.0;
			const int32 BoostPhase = BrickGuide::PhaseAfterBoost(GuidePhase, HeldSeconds);
			if (BoostPhase != GuidePhase)
			{
				SetGuidePhase(BoostPhase);
				TryCompleteByResources();
			}
		}
	}
	if (HasActiveMission() && !bReadyToClaim && !bGuideSuspendedForLaunchReward
		&& ActiveMissionRow.ConditionType == EMissionConditionType::CollectFirstRevenue
		&& FTutorialRevenueCompletionRules::CanFinish(bTutorialRevenueReceived,
			GuidePhase == CollectGuide::WatchIncome, GetPhaseElapsedSeconds()))
	{
		SetReadyToClaim();
	}
	// (폴백→임베드 승격 로직 제거 — OnLevelLayerReady 신호 뒤에 EnsureMissionWidgets 가 돌므로 레이어가 항상 준비된 상태라
	//  임베드 트래커를 처음부터 직접 잡는다. 폴백은 레이어에 임베드가 아예 없을 때만 생기는 최후수단으로, 승격 불요.)

	// CollectBricks — 패널을 먼저 연 플레이어 구제.
	if (HasActiveMission() && ActiveMissionRow.ConditionType == EMissionConditionType::CollectBricks
		&& GuidePhase == BrickGuide::OpenFactory && FactoryPanelWeak.IsValid())
	{
		SetGuidePhase(BrickGuide::AddLaborer);
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
		const UOfficeRecruitmentFlowSubsystem* Flow = GetWorld() ? GetWorld()->GetSubsystem<UOfficeRecruitmentFlowSubsystem>() : nullptr;
		if (Flow && Flow->IsBusy()) { TransitionMissCount = 0; return; }
		if (GuidePhase == RecruitGuide::PressConfirm && !GachaPresentationWeak.IsValid())
		{
			if (++TransitionMissCount >= 2) { SetGuidePhase(RecruitGuide::PressPull); }
		}
		else if ((GuidePhase == RecruitGuide::PickJob || GuidePhase == RecruitGuide::PressPull)
			&& !RecruitModalWeak.IsValid())
		{
			if (++TransitionMissCount >= 2) { SetGuidePhase(RecruitGuide::OpenRecruit); }
		}
		else
		{
			TransitionMissCount = 0;
		}
		return;
	}

	// M4 — 선택 또는 연속 배치 중 배치 모드를 나갔으면([닫기]/취소) 버튼 유도로 후퇴 (2틱 보호).
	// Confirm/PlaceMore 에서 모드가 꺼졌다 = 플레이어가 배치를 취소하거나 끝내고 나갔다는 뜻.
	if (HasActiveMission() && !bReadyToClaim
		&& ActiveMissionRow.ConditionType == EMissionConditionType::PlaceDesks
		&& (GuidePhase == DeskGuide::Confirm || GuidePhase == DeskGuide::PlaceMore))
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

	// 닫기·폐기·재접속 후에도 실제 프로젝트 상태에서 안내를 다시 구한다.
	if (HasActiveMission() && !bReadyToClaim
		&& ActiveMissionRow.ConditionType == EMissionConditionType::LaunchFirstInHouseProject)
	{
		if (HasLaunchedRequiredProject())
		{
			SetReadyToClaim();
			return;
		}
		const UOfficeStageProgressManager* StageMgr = GetWorld()->GetSubsystem<UOfficeStageProgressManager>();
		if (StageMgr)
		{
			switch (StageMgr->GetLifecycle())
			{
			case EProjectLifecycle::Idle:
				SetGuidePhase(PitchBoardWeak.IsValid() && PitchBoardWeak->IsActivated()
					? InHouseGuide::PickInHouse : InHouseGuide::OpenBoard);
				break;
			case EProjectLifecycle::Developing:
				SetGuidePhase(InHouseGuide::WatchStrip);
				break;
			case EProjectLifecycle::LaunchPending:
				SetGuidePhase(!(LaunchConfirmWeak.IsValid() && LaunchConfirmWeak->IsActivated()) ? InHouseGuide::WatchStrip
					: StageMgr->IsLaunchBlocked() ? InHouseGuide::DiscardFailed : InHouseGuide::PressLaunch);
				break;
			case EProjectLifecycle::Operating:
			case EProjectLifecycle::ReportPending:
				break;
			}
		}
		return;
	}

	if (!HasActiveMission() || ActiveMissionRow.ConditionType != EMissionConditionType::BuildFirstBuilding)
	{
		return;
	}

	if (GuidePhase == BuildGuide::PickIndustry || GuidePhase == BuildGuide::PickBuilding)
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

bool UMissionManagerSubsystem::GetMissionSequence(int32& OutCurrent, int32& OutTotal) const
{
	OutCurrent = 0;
	OutTotal = 0;
	UGameInstance* GI = GetGameInstance();
	const UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!HasActiveMission() || !TableMgr)
	{
		return false;
	}

	TSet<FName> Visited;
	FName MissionID = OpeningChainStartID;
	while (!MissionID.IsNone())
	{
		if (Visited.Contains(MissionID))
		{
			UE_LOG(LogTemp, Warning, TEXT("[MissionManager] DT_Mission 체인 순환: %s"), *MissionID.ToString());
			return false;
		}
		Visited.Add(MissionID);

		bool bFound = false;
		const FMissionTable Mission = TableMgr->GetMissionData(MissionID, bFound);
		if (!bFound)
		{
			UE_LOG(LogTemp, Warning, TEXT("[MissionManager] DT_Mission 체인 행 누락: %s"), *MissionID.ToString());
			return false;
		}

		++OutTotal;
		if (MissionID == ActiveMissionID)
		{
			OutCurrent = OutTotal;
		}
		MissionID = Mission.NextMissionID;
	}

	return OutCurrent > 0;
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

	// 조상이 숨었거나 거의 투명하면(배치 직후 상단 버튼이 다시 떠오르는 중 등) 링이 빈 배경을 가리킨다
	float Opacity = 1.f;
	const UWidget* Ancestor = Target;
	for (int32 Depth = 0; Ancestor && Depth < 64; ++Depth)
	{
		const ESlateVisibility AncestorVis = Ancestor->GetVisibility();
		if (Ancestor != Target && (AncestorVis == ESlateVisibility::Collapsed || AncestorVis == ESlateVisibility::Hidden))
		{
			OutReason = TEXT("AncestorHidden");
			return false;
		}
		Opacity *= Ancestor->GetRenderOpacity();
		// 트리의 뿌리면 그 트리를 가진 위젯으로 올라간다 ― 다른 위젯이 만들어 따로 띄운 창은 만든 쪽을 조상으로 보지 않는다
		const UWidget* Parent = Ancestor->GetParent();
		if (!Parent)
		{
			const UUserWidget* Owner = Ancestor->GetTypedOuter<UUserWidget>();
			Parent = (Owner && Owner->GetRootWidget() == Ancestor) ? Owner : nullptr;
		}
		Ancestor = Parent;
	}
	if (Opacity < 0.3f)
	{
		OutReason = TEXT("Transparent");
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
	if (!HasActiveMission() || bReadyToClaim || bGuideSuspendedForLaunchReward)
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

	// 도출형은 누를 대상이 있을 때만 막는다 — 기다림·배치 확정·설명 코치마크 걸음은 자유 조작
	if (IsDerivedGuideActive())
	{
		const FGoalGuideStep Step = GetDerivedGuideTargets();
		return Step.Anchor != nullptr && !Step.bObserve
			&& !FTutorialDerivedMissionRules::IsPlacementConfirmStep(ActiveMissionRow.ConditionType, Step.Index);
	}

	// 드래그/배치 확정 서브페이즈만 자유 조작(게이트 OFF) — 월드로 드래그·카메라 조작이 필요.
	// 배치 "버튼 누르기" 서브페이즈(PressBuild/OpenPlacement)는 게이트 ON(그 버튼만 허용).
	switch (ActiveMissionRow.ConditionType)
	{
	case EMissionConditionType::CollectBricks:
		if (GuidePhase == BrickGuide::AddLaborer || GuidePhase == BrickGuide::ExpandFactory)
		{
			if (const UCostActionButtonWidget* PurchaseButton = Cast<UCostActionButtonWidget>(GetCurrentHighlightTarget()))
			{
				if (!PurchaseButton->CanAfford()) return false;
			}
		}
		return (GuidePhase == BrickGuide::OpenFactory || GuidePhase == BrickGuide::AddLaborer
			|| GuidePhase == BrickGuide::ExpandFactory)
			&& GetCurrentHighlightTarget() != nullptr;
	case EMissionConditionType::BuildFirstBuilding:
		// 첫 건물 카드까지 누를 대상을 좁히고 월드 배치에서는 자유롭게 조작한다.
		return GuidePhase != BuildGuide::PlaceBuilding;
	case EMissionConditionType::PlaceDesks:
		// 카탈로그 선택과 배치 중엔 게이트 OFF — 카드 선택·월드 탭·카메라 조작이 필요하다.
		return GuidePhase == DeskGuide::OpenPlacement;
	case EMissionConditionType::RecruitEmployees:
		// 채용 뒤 모달이 스스로 닫히는 동안은 누를 대상이 없다 ― 구멍 없는 딤으로 화면을 잠그지 않는다
		return GuidePhase != RecruitGuide::PressCloseRecruit || GetCurrentHighlightTarget() != nullptr;
	case EMissionConditionType::CollectFirstRevenue:
		return GuidePhase != CollectGuide::WatchIncome;
	case EMissionConditionType::LaunchFirstInHouseProject:
		// 스트립은 누를 수 없는 표시용 위젯 — 게이트를 켜면 개발 중 유일한 선택지인 부스트 도박 모달까지 막혀 타임아웃된다.
		// PickInHouse 도 OFF — 3안 중 무엇을 고를지는 플레이어 몫이라 카드 한 장으로 구멍을 좁히지 않는다.
		return GuidePhase != InHouseGuide::WatchStrip && GuidePhase != InHouseGuide::PickInHouse;
	default:
		return true;
	}
}

bool UMissionManagerSubsystem::IsGuideObserveStep() const
{
	if (!HasActiveMission() || bReadyToClaim)
	{
		return false;
	}
	if (IsDerivedGuideActive())
	{
		return GetDerivedGuideTargets().bObserve;
	}
	return ActiveMissionRow.ConditionType == EMissionConditionType::LaunchFirstInHouseProject && GuidePhase == InHouseGuide::WatchStrip;
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

UWidget* UMissionManagerSubsystem::GetCurrentSecondaryHighlightTarget() const
{
	if (!HasActiveMission() || bReadyToClaim || bGuideSuspendedForLaunchReward
		|| ActiveMissionRow.ConditionType != EMissionConditionType::LaunchFirstInHouseProject
		|| GuidePhase != InHouseGuide::WatchStrip)
	{
		return nullptr;
	}
	return OfficeMainWeak.IsValid() ? OfficeMainWeak->GetStripRequirementRowWidget() : nullptr;
}

UWidget* UMissionManagerSubsystem::GetCurrentHighlightTarget() const
{
	// 클레임 대기 중엔 유도 없음 — 카드 탭만 기다린다
	if (!HasActiveMission() || bReadyToClaim || bGuideSuspendedForLaunchReward)
	{
		return nullptr;
	}
	if (IsDerivedGuideActive())
	{
		return GetDerivedGuideTargets().Anchor;
	}

	// 벽돌 생산 유도 — 공장 버튼과 인부 추가 슬롯만 링으로 표시한다.
	if (ActiveMissionRow.ConditionType == EMissionConditionType::CollectBricks)
	{
		if (GuidePhase == BrickGuide::OpenFactory)
		{
			// [공장] 버튼 — 공장 패널 열기 유도
			return BuildOpenWidgetWeak.IsValid() ? BuildOpenWidgetWeak->GetFactoryOpenButtonWidget() : nullptr;
		}
		if (GuidePhase == BrickGuide::AddLaborer || GuidePhase == BrickGuide::ExpandFactory)
		{
			// 패널을 닫았다면 [공장] 버튼으로 다시 안내한다.
			if (FactoryPanelWeak.IsValid())
			{
				UWidget* Nudge = GuidePhase == BrickGuide::AddLaborer
					? FactoryPanelWeak->GetLaborerUpgradeTarget()
					: FactoryPanelWeak->GetFactoryExpansionUpgradeTarget();
				if (Nudge)
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
		const UOfficeRecruitmentFlowSubsystem* Flow = GetWorld() ? GetWorld()->GetSubsystem<UOfficeRecruitmentFlowSubsystem>() : nullptr;
		if (Flow && Flow->IsBusy()) return nullptr;
		if (GuidePhase == RecruitGuide::OpenRecruit)
			return OfficeMainWeak.IsValid() ? OfficeMainWeak->GetRecruitmentButtonWidget() : nullptr;
		if (GuidePhase == RecruitGuide::PickJob)
			return RecruitModalWeak.IsValid() ? RecruitModalWeak->GetJobSelectionWidget(GetRequiredRecruitDiscipline()) : nullptr;
		if (GuidePhase == RecruitGuide::PressPull)
			return RecruitModalWeak.IsValid() ? RecruitModalWeak->GetPullButtonMultiWidget() : nullptr;
		if (GuidePhase == RecruitGuide::PressConfirm)
			return GachaPresentationWeak.IsValid() ? GachaPresentationWeak->GetConfirmButtonWidget() : nullptr;
		// PressCloseRecruit 는 링 대상이 없다 ― 채용 모달은 고용 확정 뒤 스스로 닫힌다
		return nullptr;
	}

	// M4 PlaceDesks — p0 [책상 배치] 버튼 / p1 카탈로그 자유 선택 / p2·p3 배치 바 [배치] 링
	if (ActiveMissionRow.ConditionType == EMissionConditionType::PlaceDesks)
	{
		if (GuidePhase == DeskGuide::OpenPlacement)
			return OfficeMainWeak.IsValid() ? OfficeMainWeak->GetWorkstationOpenButtonWidget() : nullptr;
		if (GuidePhase == DeskGuide::PickDesk)
		{
			UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
			UUIBase* Base = UIMgr ? UIMgr->GetUIBase() : nullptr;
			UOfficeWorkstationPanelWidget* Catalog = Base ? Cast<UOfficeWorkstationPanelWidget>(Base->GetActiveBottomWidget()) : nullptr;
			return Catalog ? Catalog->GetDeskGuideCatalogWidget() : nullptr;
		}
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
			return OfficeMainWeak.IsValid() ? OfficeMainWeak->GetStripRequirementRowWidget() : nullptr;
		if (GuidePhase == InHouseGuide::PressLaunch)
			return LaunchConfirmWeak.IsValid() ? LaunchConfirmWeak->GetConfirmButtonWidget() : nullptr;
		if (GuidePhase == InHouseGuide::DiscardFailed)
			return LaunchConfirmWeak.IsValid() ? LaunchConfirmWeak->GetDiscardButtonWidget() : nullptr;
		return nullptr;
	}

	// M7 CollectFirstRevenue — p0 사무실 [뒤로] / p1 순차 설명 / p2 수익 관찰(타겟 없음)
	if (ActiveMissionRow.ConditionType == EMissionConditionType::CollectFirstRevenue)
	{
		if (GuidePhase == CollectGuide::ExitOffice)
			return OfficeLayerWeak.IsValid() ? OfficeLayerWeak->GetBackButtonWidget() : nullptr;
		if (GuidePhase == CollectGuide::WatchIncome)
		{
			UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
			UInGameLayerWidget* Layer = UIMgr ? UIMgr->GetInGameLayer() : nullptr;
			return Layer ? Layer->GetRevenueRateChipWidget() : nullptr;
		}
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
	if (GuidePhase == BuildGuide::PickIndustry)
	{
		UBuildModalWidget* Catalog = Cast<UBuildModalWidget>(BuildModalWeak.Get());
		return Catalog ? Catalog->GetBuildGuideIndustryWidget() : nullptr;
	}
	if (GuidePhase == BuildGuide::PickBuilding)
	{
		UBuildModalWidget* Catalog = Cast<UBuildModalWidget>(BuildModalWeak.Get());
		return Catalog ? Catalog->GetBuildGuideFirstCardWidget() : nullptr;
	}
	return nullptr;
}

AActor* UMissionManagerSubsystem::GetCurrentSpotlightActor() const
{
	if (!HasActiveMission() || bReadyToClaim)
	{
		return nullptr;
	}
	if (IsDerivedGuideActive())
	{
		return GetDerivedGuideTargets().Spot;
	}

	// 자동 생산 관찰과 홀드 학습에서 공장을 밝힌다. 관찰 단계의 손 제스처는 표시하지 않는다.
	if (ActiveMissionRow.ConditionType == EMissionConditionType::CollectBricks
		&& (GuidePhase == BrickGuide::ObserveLaborers || GuidePhase == BrickGuide::WatchExpansion
			|| GuidePhase == BrickGuide::BoostLaborers))
	{
		if (!BrickFactoryWeak.IsValid())
		{
			BrickFactoryWeak = UGameplayStatics::GetActorOfClass(
				GetGameInstance()->GetWorld(), ABrickFactory::StaticClass());
		}
		return BrickFactoryWeak.Get();
	}
	// M3 EnterOffice — 지은 회사 빌딩 스포트라이트 (ClickBuilding 페이즈, MainMap).
	// 새 월드에서는 저장된 회사 목록으로 재탐색한다.
	if (ActiveMissionRow.ConditionType == EMissionConditionType::EnterOffice
		&& GuidePhase == EnterOfficeGuide::ClickBuilding)
	{
		return TutorialFirstBuildingWeak.IsValid() ? TutorialFirstBuildingWeak.Get() : PickGuideBuilding();
	}

	return nullptr;
}

EGestureHintKind UMissionManagerSubsystem::GetCurrentGesture() const
{
	if (!HasActiveMission() || bReadyToClaim) return EGestureHintKind::None;
	if (IsDerivedGuideActive())
	{
		// 페이즈는 0.25초 늦게 따라오므로 손 모양도 지금 걸음에서 정한다. 월드 대상은 탭, 버튼은 링이 맡는다
		const FGoalGuideStep Step = GetDerivedGuideTargets();
		if (FTutorialDerivedMissionRules::IsPlacementConfirmStep(ActiveMissionRow.ConditionType, Step.Index))
		{
			return EGestureHintKind::Drag;
		}
		return Step.Spot && !Step.bObserve ? EGestureHintKind::Tap : EGestureHintKind::None;
	}
	return GuideGestureRules::ResolveGesture(ActiveMissionRow.ConditionType, GuidePhase);
}

float UMissionManagerSubsystem::GetGuideDimScale() const
{
	if (!HasActiveMission() || bReadyToClaim) return 1.f;
	if (IsDerivedGuideActive())
	{
		const FGoalGuideStep Step = GetDerivedGuideTargets();
		return Step.bObserve || FTutorialDerivedMissionRules::IsPlacementConfirmStep(ActiveMissionRow.ConditionType, Step.Index) ? 0.f : 1.f;
	}
	return GuideGestureRules::ResolveDimScale(ActiveMissionRow.ConditionType, GuidePhase);
}

AActor* UMissionManagerSubsystem::GetCurrentGestureAnchorActor() const
{
	if (!HasActiveMission() || bReadyToClaim) return nullptr;
	if (IsDerivedGuideActive())
	{
		const FGoalGuideStep Step = GetDerivedGuideTargets();
		if (FTutorialDerivedMissionRules::IsPlacementConfirmStep(ActiveMissionRow.ConditionType, Step.Index))
		{
			return FindPlacementPreviewActor(GetGameInstance());
		}
		return Step.Spot;
	}
	switch (GuideGestureRules::ResolveAnchor(ActiveMissionRow.ConditionType, GuidePhase))
	{
	case EGestureAnchorKind::SpotlightActor:
		return GetCurrentSpotlightActor();
	case EGestureAnchorKind::BrickFactory:
		if (!BrickFactoryWeak.IsValid())
		{
			BrickFactoryWeak = UGameplayStatics::GetActorOfClass(GetGameInstance()->GetWorld(), ABrickFactory::StaticClass());
		}
		return BrickFactoryWeak.Get();
	case EGestureAnchorKind::PlacementPreview:
		return FindPlacementPreviewActor(GetGameInstance());
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
	if (!ActiveMissionRow.ExplainLines.IsValidIndex(Step)
		|| ActiveMissionRow.ExplainLines[Step].IsEmpty())
	{
		return false;
	}

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
	OutCopy.Body = ActiveMissionRow.ExplainLines[Step];
	OutCopy.Dir = Step == 0 ? EGuideTooltipDir::Up : EGuideTooltipDir::Right;
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
		SetGuidePhase(CollectGuide::WatchIncome);
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
		SetGuidePhase(CollectGuide::WatchIncome);
	}
	else
	{
		ExplainStep = StepAfterSkipped;
	}
}

bool UMissionManagerSubsystem::HasLaunchedRequiredProject() const
{
	USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>();
	const USaveGame_GameData* Save = SaveMgr ? SaveMgr->GetCurrentSaveData() : nullptr;
	if (!Save || ActiveMissionRow.RequiredProjectIndex <= 0) return false;
	const UCGGameInstance* CGInstance = Cast<UCGGameInstance>(GetGameInstance());
	int32 BuildingIndex = CGInstance ? CGInstance->GetCurrentManagedBuildingIndex() : INDEX_NONE;
	if (BuildingIndex == INDEX_NONE && Save->GameData.Buildings.Num() == 1)
	{
		BuildingIndex = Save->GameData.Buildings[0].BuildingIndex;
	}
	for (const FBuildingEntitySaveData& Building : Save->GameData.Buildings)
	{
		if (Building.BuildingIndex == BuildingIndex)
		{
			return FTutorialMissionRestoreRules::HasRequiredLaunch(Save->GameData.ShippedProjects,
				BuildingIndex, Building.BuildingData.CompanyType, ActiveMissionRow.RequiredProjectIndex);
		}
	}
	return false;
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

// ===== 도출형 튜토리얼 (M8~M31) =====

int32 UMissionManagerSubsystem::GetTutorialCompanyIndex(int32 Order) const
{
	UGameInstance* GI = GetGameInstance();
	USaveLoadManager* SaveMgr = GI ? GI->GetSubsystem<USaveLoadManager>() : nullptr;
	const USaveGame_GameData* Save = SaveMgr ? SaveMgr->GetCurrentSaveData() : nullptr;
	TArray<int32> OwnedIndices;
	if (Save)
	{
		// 공장은 Buildings 가 아니라 FactoryData 에 있어 회사만 남는다
		for (const FBuildingEntitySaveData& Building : Save->GameData.Buildings)
		{
			OwnedIndices.Add(Building.BuildingIndex);
		}
	}
	// 막 지은 회사는 다음 저장 전까지 세이브에 없다 — 이 맵의 회사도 합친다
	UWorld* CurWorld = GetWorld();
	if (const UEntityManager* Entities = CurWorld ? CurWorld->GetSubsystem<UEntityManager>() : nullptr)
	{
		for (const ABuildingBaseActor* Building : Entities->GetBuildings())
		{
			if (IsValid(Building))
			{
				OwnedIndices.Add(Building->GetBuildingIndex());
			}
		}
	}
	const int32 CompanyIndex = FTutorialDerivedMissionRules::ResolveCompanyIndexByOrder(OwnedIndices, Order);
	if (CompanyIndex != INDEX_NONE || Order > 1)
	{
		return CompanyIndex;
	}
	const ABuildingBaseActor* Fallback = PickGuideBuilding();
	return Fallback ? Fallback->GetBuildingIndex() : INDEX_NONE;
}

int32 UMissionManagerSubsystem::GetActiveTutorialCompanyIndex() const
{
	return GetTutorialCompanyIndex(IsDerivedGuideActive() ? ActiveMissionRow.TutorialCompanyOrder : 1);
}

const FBuildingEntitySaveData* UMissionManagerSubsystem::FindTutorialCompanySave(int32 CompanyIndex) const
{
	UGameInstance* GI = GetGameInstance();
	USaveLoadManager* SaveMgr = GI ? GI->GetSubsystem<USaveLoadManager>() : nullptr;
	const USaveGame_GameData* Save = SaveMgr ? SaveMgr->GetCurrentSaveData() : nullptr;
	if (!Save || CompanyIndex == INDEX_NONE)
	{
		return nullptr;
	}
	return Save->GameData.Buildings.FindByPredicate([CompanyIndex](const FBuildingEntitySaveData& Building)
	{
		return Building.BuildingIndex == CompanyIndex;
	});
}

ABuildingBaseActor* UMissionManagerSubsystem::FindTutorialCompanyActor(int32 CompanyIndex) const
{
	UWorld* CurWorld = GetWorld();
	UEntityManager* Entities = CurWorld ? CurWorld->GetSubsystem<UEntityManager>() : nullptr;
	return Entities && CompanyIndex != INDEX_NONE ? Entities->GetBuildingByIndex(CompanyIndex) : nullptr;
}

FContentGuideSpec UMissionManagerSubsystem::MakeActiveMissionSpec() const
{
	FContentGuideSpec Spec;
	if (!HasActiveMission())
	{
		return Spec;
	}
	Spec.Condition = ActiveMissionRow.ConditionType;
	Spec.BuildingIndex = GetActiveTutorialCompanyIndex();
	Spec.Discipline = ActiveMissionRow.RequiredRecruitDiscipline;
	Spec.TargetProjectIndex = ActiveMissionRow.RequiredProjectIndex;
	Spec.ConditionAmount = ActiveMissionRow.ConditionAmount;
	Spec.Industry = ActiveMissionRow.RequiredIndustry;
	if (Spec.Industry == ECompanyType::None)
	{
		const int32 FirstCompanyIndex = GetTutorialCompanyIndex();
		if (const FBuildingEntitySaveData* FirstCompany = FindTutorialCompanySave(FirstCompanyIndex))
		{
			Spec.Industry = FirstCompany->BuildingData.CompanyType;
		}
		else if (const ABuildingBaseActor* FirstActor = FindTutorialCompanyActor(FirstCompanyIndex))
		{
			Spec.Industry = FirstActor->GetCompanyType();
		}
	}
	Spec.bForcedTutorial = true;
	return Spec;
}

FGoalGuideStep UMissionManagerSubsystem::GetDerivedStep() const
{
	return IsDerivedGuideActive() ? ResolveContentStep(MakeActiveMissionSpec()) : FGoalGuideStep();
}

FGoalGuideStep UMissionManagerSubsystem::GetDerivedGuideTargets() const
{
	if (!IsDerivedGuideActive() || bReadyToClaim || IsPanelIntroPlaying())
	{
		return FGoalGuideStep();
	}
	return GetDerivedStep();
}

bool UMissionManagerSubsystem::IsPanelIntroPlaying() const
{
	const UGameInstance* GI = GetGameInstance();
	const UPanelIntroSubsystem* IntroMgr = GI ? GI->GetSubsystem<UPanelIntroSubsystem>() : nullptr;
	return IntroMgr && IntroMgr->IsIntroPlaying();
}

bool UMissionManagerSubsystem::HasDerivedLatch(const TCHAR* LatchName) const
{
	return LatchName && DerivedMissionLatches.Contains(FName(LatchName));
}

bool UMissionManagerSubsystem::IsCompanyEnhancementUnlocked() const
{
	return FTutorialEnhancementGateRules::IsCompanyEnhancementUnlocked(bTutorialCompleted, IsDerivedGuideActive());
}

void UMissionManagerSubsystem::AddDerivedLatch(const TCHAR* LatchName)
{
	if (!LatchName || !IsDerivedGuideActive())
	{
		return;
	}
	const FName Latch(LatchName);
	if (DerivedMissionLatches.Contains(Latch))
	{
		return;
	}
	DerivedMissionLatches.Add(Latch);
	UE_LOG(LogTemp, Log, TEXT("[MissionGuide] 래치 기록 ― %s %s"), *ActiveMissionID.ToString(), LatchName);
	// 이벤트형은 그 순간을 놓치면 흔적이 없어 래치 시점에 저장을 건다
	if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
	{
		SaveMgr->RequestDeferredSave();
	}
}

void UMissionManagerSubsystem::HandlePanelIntroCompleted(FName PanelKey)
{
	if (!IsDerivedGuideActive() || bReadyToClaim)
	{
		return;
	}
	if (const TCHAR* Latch = FTutorialDerivedMissionRules::ResolveIntroLatch(ActiveMissionRow.ConditionType, PanelKey))
	{
		AddDerivedLatch(Latch);
		EvaluateDerivedMissionCompletion();
	}
}

void UMissionManagerSubsystem::PollDerivedGuide()
{
	const FGoalGuideStep Step = GetDerivedStep();
	if (UScrollBox* Scroll = FindGuideScrollBox(Step.Anchor))
	{
		Scroll->ScrollWidgetIntoView(Step.Anchor, true, EDescendantScrollDestination::IntoView);
	}
	// 문구·트래커 채널은 페이즈를 본다. 대상은 게터가 매 틱 다시 구하므로 여기서는 번호만 맞춘다
	if (Step.Index != INDEX_NONE && Step.Index != GuidePhase)
	{
		SetGuidePhase(Step.Index);
	}
	// 인수할 회사·철거한 부지는 첫 화면 밖에 있다 — 셰브론만 보고 찾아가게 두지 않고 대상이 바뀔 때 한 번 데려간다
	if (Step.Spot != DerivedFocusedSpotWeak.Get())
	{
		const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
		if (Step.Spot != DerivedPendingSpotWeak.Get())
		{
			DerivedPendingSpotWeak = Step.Spot;
			DerivedSpotSeenAt = Now;
		}
		const bool bLoadingShown = IsMoviePlayerEnabled() && GetMoviePlayer()->IsMovieCurrentlyPlaying();
		if (bLoadingShown)
		{
			DerivedSpotSeenAt = Now;
		}
		APlayerCamera* Camera = IsValid(Step.Spot) && !bLoadingShown && Now - DerivedSpotSeenAt >= 0.5
			? FindPlayerCamera(GetWorld()) : nullptr;
		if (!Step.Spot || Camera)
		{
			DerivedFocusedSpotWeak = Step.Spot;
			if (Camera)
			{
				Camera->FocusOnActor(Step.Spot, 0.5f, false);
			}
		}
	}
	UpdateDerivedPresentation(Step);
	EvaluateDerivedMissionCompletion();
}

void UMissionManagerSubsystem::UpdateDerivedPresentation(const FGoalGuideStep& Step)
{
	UWorld* CurWorld = GetWorld();
	const double Now = CurWorld ? CurWorld->GetTimeSeconds() : 0.0;

	// 설명 패널이 닫혔으면 다시 열 때 코치마크를 처음부터 띄울 수 있게 기록을 지운다
	const UCommonActivatableWidget* IntroPanel = Cast<UCommonActivatableWidget>(DerivedIntroPanelWeak.Get());
	if (!IntroPanel || !IntroPanel->IsActivated())
	{
		DerivedIntroPanelWeak.Reset();
	}

	UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
	UUIBase* UIBase = UIMgr ? UIMgr->GetUIBase() : nullptr;
	UCommonActivatableWidget* TopPrompt = UIBase ? UIBase->GetActivePromptWidget() : nullptr;

	// 설명 걸음에 들어오면 패널 코치마크를 한 번 띄운다. 기록 없이 끝나면(앵커 누락 등) 같은 패널에 무한 재생하지 않고 통과시킨다
	// 보상 띠가 지나가는 동안 코치마크를 시작하면 첫 장이 띠 밑에서 지나간다 ― 띠가 걷힌 뒤 시작한다
	const bool bRewardToastShown = UIMgr && UIMgr->IsRewardToastShown();
	auto PlayIntroOnce = [this, bRewardToastShown](auto* Panel, const TCHAR* Latch)
	{
		if (!Panel || !Panel->IsActivated() || HasDerivedLatch(Latch) || Panel->IsPanelIntroPlaying() || bRewardToastShown)
		{
			return;
		}
		if (DerivedIntroPanelWeak.Get() == Panel)
		{
			UE_LOG(LogTemp, Warning, TEXT("[MissionGuide] 설명 코치마크가 완료 기록 없이 끝났습니다 ― %s %s 걸음을 통과로 처리합니다"),
				*ActiveMissionID.ToString(), Latch);
			AddDerivedLatch(Latch);
			return;
		}
		DerivedIntroPanelWeak = Panel;
		if (!Panel->PlayPanelIntro())
		{
			UE_LOG(LogTemp, Warning, TEXT("[MissionGuide] 설명 코치마크를 시작하지 못했습니다(DT_PanelIntro 확인) ― %s %s 걸음을 통과로 처리합니다"),
				*ActiveMissionID.ToString(), Latch);
			AddDerivedLatch(Latch);
		}
	};

	switch (ActiveMissionRow.ConditionType)
	{
	case EMissionConditionType::ExplainPlot:
	{
		// 철거한 회사가 있던 부지의 구매 창을 일정 시간 읽어야 설명을 본 것으로 친다
		ACityPlotActor* Plot = FindTutorialClearedPlot(CurWorld);
		// 「이 땅의 회사를 모두 비우면」이 어느 땅인지 보이게 설명 내내 부지 바닥을 밝혀 둔다
		if (Plot)
		{
			Plot->HoldSelectionHighlight();
		}
		if (!Plot || !Plot->IsPurchaseDialogOpen())
		{
			DerivedPlotDialogOpenedAt = -1.0;
			break;
		}
		if (DerivedPlotDialogOpenedAt < 0.0)
		{
			DerivedPlotDialogOpenedAt = Now;
		}
		if (FTutorialDerivedMissionRules::ShouldLatchPlotExplain(DerivedPlotDialogOpenedAt, Now))
		{
			AddDerivedLatch(TutorialDerivedLatch::PlotDialogSeen);
		}
		break;
	}
	case EMissionConditionType::ReachHQLevel:
		if (Step.Index == HQLevelUpGuide::Explain)
		{
			PlayIntroOnce(Cast<UHQManagePanelWidget>(TopPrompt), TutorialDerivedLatch::HQIntroDone);
		}
		break;
	case EMissionConditionType::MenuTour:
		if (Step.Index == MenuTourGuide::ExplainMenu)
		{
			PlayIntroOnce(Cast<UMenuPanelWidget>(TopPrompt), TutorialDerivedLatch::MenuIntroDone);
		}
		break;
	default:
		break;
	}
}

void UMissionManagerSubsystem::EvaluateDerivedMissionCompletion()
{
	if (!IsDerivedGuideActive() || bReadyToClaim || bCompletingMission)
	{
		return;
	}
	// 같은 프레임에 이미 한 미션이 넘어갔으면 다음 폴에서 본다 — 설명 없이 연달아 건너뛰는 미션을 로그로 드러내려고
	if (LastMissionAdvanceFrame == GFrameCounter)
	{
		return;
	}
	const bool bFirstEvaluation = !bDerivedEvaluatedOnce;
	bDerivedEvaluatedOnce = true;
	// 레벨업 보상 지급의 자원 변경이 레벨업 신호보다 먼저 판정을 부를 수 있다 — 오른 레벨을 처음 본 순간부터 연출 시간을 잰다
	if (!bFirstEvaluation && ActiveMissionRow.ConditionType == EMissionConditionType::ReachHQLevel && DerivedHQLevelSignalAt < 0.0)
	{
		USaveLoadManager* SaveMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<USaveLoadManager>() : nullptr;
		if (SaveMgr && SaveMgr->GetHQLevel() >= FTutorialDerivedMissionRules::ResolveTarget(ActiveMissionRow.ConditionAmount))
		{
			DerivedHQLevelSignalAt = GetWorld() ? GetWorld()->GetTimeSeconds() : -1.0;
		}
	}
	if (ActiveMissionRow.ConditionType == EMissionConditionType::EquipFirstEquipment && !HasDerivedLatch(TutorialDerivedLatch::EquipBaseline))
	{
		TArray<FGuid> Worn;
		CollectEquippedInBuilding(GetGameInstance(), GetActiveTutorialCompanyIndex(), Worn);
		for (const FGuid& InstanceId : Worn)
		{
			AddDerivedLatch(*FTutorialDerivedMissionRules::MakeEquipHadLatch(InstanceId).ToString());
		}
		AddDerivedLatch(TutorialDerivedLatch::EquipBaseline);
	}
	if (!IsDerivedMissionConditionMet())
	{
		return;
	}
	if (bFirstEvaluation)
	{
		UE_LOG(LogTemp, Log, TEXT("[MissionGuide] 이미 충족된 미션을 건너뜁니다 ― %s"), *ActiveMissionID.ToString());
	}
	SetReadyToClaim();
}

bool UMissionManagerSubsystem::IsDerivedMissionConditionMet() const
{
	UGameInstance* GI = GetGameInstance();
	UCGGameInstance* CGInstance = Cast<UCGGameInstance>(GI);
	UWorld* CurWorld = GetWorld();
	USaveLoadManager* SaveMgr = GI ? GI->GetSubsystem<USaveLoadManager>() : nullptr;
	const USaveGame_GameData* Save = SaveMgr ? SaveMgr->GetCurrentSaveData() : nullptr;
	const UEmployeeManager* Employees = GI ? GI->GetSubsystem<UEmployeeManager>() : nullptr;
	const int64 Amount = ActiveMissionRow.ConditionAmount;
	const int32 Target = FTutorialDerivedMissionRules::ResolveTarget(Amount);
	const double Now = CurWorld ? CurWorld->GetTimeSeconds() : 0.0;
	const int32 CompanyIndex = GetActiveTutorialCompanyIndex();
	const ABuildingBaseActor* CompanyActor = FindTutorialCompanyActor(CompanyIndex);
	const FBuildingEntitySaveData* CompanySave = FindTutorialCompanySave(CompanyIndex);
	// 사무실 실물 책상은 그 회사 사무실에 있을 때만 믿는다. 그 밖에서는 저장된 배치를 센다
	const UOfficeManager* LiveOffice = CurWorld && CGInstance && CGInstance->IsInOfficeMap()
		&& CGInstance->GetCurrentManagedBuildingIndex() == CompanyIndex
		? CurWorld->GetSubsystem<UOfficeManager>() : nullptr;
	const FOfficeSaveData* SavedOffice = Save && CompanyIndex != INDEX_NONE
		? Save->GameData.OfficeDataMap.Find(CompanyIndex) : nullptr;

	switch (ActiveMissionRow.ConditionType)
	{
	case EMissionConditionType::UpgradeFactoryStats:
	{
		if (!BrickFactoryWeak.IsValid() && CurWorld && !(CGInstance && CGInstance->IsInOfficeMap()))
		{
			BrickFactoryWeak = UGameplayStatics::GetActorOfClass(CurWorld, ABrickFactory::StaticClass());
		}
		const ABrickFactory* Factory = Cast<ABrickFactory>(BrickFactoryWeak.Get());
		// 패널까지 닫혀야 끝 — 열린 패널이 다음 미션의 도크 버튼을 덮는다
		return Factory && !FactoryPanelWeak.IsValid() && FTutorialDerivedMissionRules::AreFactoryStatsReached(
			Factory->GetUpgradeLevel(EFactoryUpgradeType::HoldProductionSpeed),
			Factory->GetUpgradeLevel(EFactoryUpgradeType::HoldProductionAmount),
			Amount);
	}
	case EMissionConditionType::RaiseBuildingFloor:
	{
		const int32 FloorLevel = CompanyActor
			? CompanyActor->GetEnhancementLevel(EBuildingEnhancementType::BuildingFloor)
			: (CompanySave ? CompanySave->BuildingData.Body_Module_Copies : 0);
		const UBuildingManagePanelWidget* Manage = ManagePanelWeak.Get();
		return FTutorialDerivedMissionRules::IsFloorRaiseDone(
			FloorLevel, Amount, DerivedFloorSignalAt, Now, Manage && Manage->IsActivated());
	}
	case EMissionConditionType::EnhanceBuildingStat:
	{
		// 빌드업(층)은 M9 몫이라 세지 않는다
		if (CompanyActor)
		{
			const UEnum* EnhanceEnum = StaticEnum<EBuildingEnhancementType>();
			for (int32 EnumIndex = 0; EnhanceEnum && EnumIndex < EnhanceEnum->NumEnums() - 1; ++EnumIndex)
			{
				const EBuildingEnhancementType Type = static_cast<EBuildingEnhancementType>(EnhanceEnum->GetValueByIndex(EnumIndex));
				if (Type != EBuildingEnhancementType::BuildingFloor && CompanyActor->GetEnhancementLevel(Type) >= Target)
				{
					return true;
				}
			}
			return false;
		}
		if (CompanySave)
		{
			for (const TPair<EBuildingEnhancementType, int32>& Entry : CompanySave->BuildingData.EnhancementLevels)
			{
				if (Entry.Key != EBuildingEnhancementType::BuildingFloor && Entry.Value >= Target)
				{
					return true;
				}
			}
		}
		return false;
	}
	case EMissionConditionType::EquipFirstTrait:
	{
		const UBuildingTraitManagerSubsystem* TraitMgr = GI ? GI->GetSubsystem<UBuildingTraitManagerSubsystem>() : nullptr;
		if (!TraitMgr || CompanyIndex == INDEX_NONE)
		{
			return false;
		}
		// 조회 결과는 빈 칸(None)까지 최대 칸 수로 채워 온다 — 실제로 낀 특성만 센다
		int32 Equipped = 0;
		for (const FName& TraitId : TraitMgr->GetEquippedTraits(CompanyIndex))
		{
			if (!TraitId.IsNone()) ++Equipped;
		}
		return Equipped >= Target;
	}
	case EMissionConditionType::EquipFirstSkin:
	{
		const UBuildingSkinManagerSubsystem* SkinMgr = GI ? GI->GetSubsystem<UBuildingSkinManagerSubsystem>() : nullptr;
		if (!SkinMgr)
		{
			return false;
		}
		// 배치 때 붙는 건물별 기본 스킨은 소유 목록에 없다 — 소유한 가챠 스킨이 걸려 있어야 「기본 스킨이 아니다」
		const int32 AppliedSkin = CompanyActor ? CompanyActor->GetAppliedSkinID()
			: (CompanySave ? CompanySave->BuildingData.AppliedSkinID : 0);
		const bool bWearsGachaSkin = AppliedSkin > 0 && AppliedSkin != TutorialStarterSkinID && SkinMgr->IsSkinOwned(AppliedSkin);
		return bWearsGachaSkin || (HasDerivedLatch(TutorialDerivedLatch::SkinApplied) && SkinMgr->HasAnyGachaSkin());
	}
	case EMissionConditionType::PlaceSecondDesk:
	{
		int32 Seats = 0;
		if (LiveOffice)
		{
			for (const AWorkstationActorBase* Desk : LiveOffice->GetPlacedWorkstations())
			{
				Seats += IsValid(Desk) ? Desk->GetChairCount() : 0;
			}
		}
		else if (SavedOffice)
		{
			for (const FWorkstationSaveData& Desk : SavedOffice->OfficeWorkstations)
			{
				Seats += Desk.ChairStates.Num();
			}
		}
		return Seats >= Target;
	}
	case EMissionConditionType::RecruitGoalDiscipline:
	{
		// 사원증 리빌 도중 다음 미션으로 넘어가 연출을 끊지 않는다
		const UOfficeRecruitmentFlowSubsystem* Flow = CurWorld ? CurWorld->GetSubsystem<UOfficeRecruitmentFlowSubsystem>() : nullptr;
		if ((Flow && Flow->IsBusy()) || bRecruitmentPresentationActive || !Employees || CompanyIndex == INDEX_NONE)
		{
			return false;
		}
		int32 Count = 0;
		for (const FEmployeeInstance& Employee : Employees->GetEmployeesInBuilding(CompanyIndex))
		{
			if (GetPrimaryDisciplineSlot(Employee) == static_cast<int32>(ActiveMissionRow.RequiredRecruitDiscipline))
			{
				++Count;
			}
		}
		return Count >= Target;
	}
	case EMissionConditionType::LevelUpFirstEmployee:
		if (Employees && CompanyIndex != INDEX_NONE)
		{
			for (const FEmployeeInstance& Employee : Employees->GetEmployeesInBuilding(CompanyIndex))
			{
				if (Employee.Level >= Target)
				{
					return true;
				}
			}
		}
		return false;
	case EMissionConditionType::AllocateEmployeeStatPoint:
		return HasDerivedLatch(TutorialDerivedLatch::StatPointAllocated);
	case EMissionConditionType::EnhanceFirstEmployee:
		return HasDerivedLatch(TutorialDerivedLatch::StarEnhanceAttempted);
	case EMissionConditionType::EquipFirstEquipment:
	{
		if (!HasDerivedLatch(TutorialDerivedLatch::EquipBaseline))
		{
			return false;
		}
		TArray<FGuid> Worn;
		CollectEquippedInBuilding(GI, CompanyIndex, Worn);
		return FTutorialDerivedMissionRules::CountNewlyWornEquipment(Worn, DerivedMissionLatches) >= Target;
	}
	case EMissionConditionType::UpgradeFirstWorkstation:
	{
		constexpr EWorkstationUpgradeType GrowthTypes[] = {
			EWorkstationUpgradeType::Processing, EWorkstationUpgradeType::Tools, EWorkstationUpgradeType::Environment };
		if (LiveOffice)
		{
			for (const AWorkstationActorBase* Desk : LiveOffice->GetPlacedWorkstations())
			{
				for (const EWorkstationUpgradeType Type : GrowthTypes)
				{
					if (IsValid(Desk) && Desk->GetUpgradeLevel(Type) >= Target)
					{
						return true;
					}
				}
			}
			return false;
		}
		if (SavedOffice)
		{
			for (const FWorkstationSaveData& Desk : SavedOffice->OfficeWorkstations)
			{
				for (const EWorkstationUpgradeType Type : GrowthTypes)
				{
					// 저장 맵에 없는 항목은 기본 Lv1 (AWorkstationActorBase::GetUpgradeLevel 과 같은 규칙)
					if (FMath::Max(1, Desk.UpgradeLevels.FindRef(Type)) >= Target)
					{
						return true;
					}
				}
			}
		}
		return false;
	}
	case EMissionConditionType::PlaceFirstPainting:
		// 스타터 프리셋 그림과 구분할 수 없어 저장된 그림 수로는 판정하지 않는다
		return HasDerivedLatch(TutorialDerivedLatch::PaintingPlaced);
	case EMissionConditionType::LaunchGoalProject:
		return Save && CompanySave && FTutorialMissionRestoreRules::HasRequiredLaunch(Save->GameData.ShippedProjects,
			CompanyIndex, CompanySave->BuildingData.CompanyType, ActiveMissionRow.RequiredProjectIndex);
	// 배치 미리보기도 건물 목록에 잠깐 올라온다 — 확정해 저장된 건물만 세고, 배치 중엔 끝내지 않는다
	case EMissionConditionType::BuildSecondBuilding:
		return GetTutorialPlacementMode(GI) != EPlacementMode::Building && Save && Save->GameData.Buildings.Num() >= Target;
	case EMissionConditionType::BuildIndustry:
	{
		const ECompanyType Industry = MakeActiveMissionSpec().Industry;
		int32 SavedCount = 0;
		if (Save)
		{
			for (const FBuildingEntitySaveData& Building : Save->GameData.Buildings)
			{
				SavedCount += Building.BuildingData.CompanyType == Industry ? 1 : 0;
			}
		}
		return Industry != ECompanyType::None && GetTutorialPlacementMode(GI) != EPlacementMode::Building && SavedCount >= Target;
	}
	case EMissionConditionType::AcquireFirstCompany:
		return HasCityCompanyInState(CurWorld, Save, EAcqState::Milking)
			|| HasCityCompanyInState(CurWorld, Save, EAcqState::Depleted)
			|| HasCityCompanyInState(CurWorld, Save, EAcqState::Cleared);
	case EMissionConditionType::SkipCompanyRecovery:
		return HasCityCompanyInState(CurWorld, Save, EAcqState::Depleted)
			|| HasCityCompanyInState(CurWorld, Save, EAcqState::Cleared);
	case EMissionConditionType::DemolishFirstCompany:
		return FTutorialDerivedMissionRules::IsDemolishDone(
			HasCityCompanyInState(CurWorld, Save, EAcqState::Cleared), DerivedDemolishSignalAt, Now);
	case EMissionConditionType::ExplainPlot:
		// 열람 시각은 창이 닫히면 -1 로 돌아간다 — 다 읽고 닫았을 때 끝난다
		return FTutorialDerivedMissionRules::IsPlotExplainDone(
			HasDerivedLatch(TutorialDerivedLatch::PlotDialogSeen), DerivedPlotDialogOpenedAt >= 0.0);
	case EMissionConditionType::ReachHQLevel:
	{
		// 축하 창을 닫을 때까지 머문다 ― 다음 미션이 「먼저 이 창을 닫아요」로 해금 소식을 덮지 않게
		UUIManagerSubsystem* UIMgr = GI ? GI->GetSubsystem<UUIManagerSubsystem>() : nullptr;
		UUIBase* UIBase = UIMgr ? UIMgr->GetUIBase() : nullptr;
		const UCommonActivatableWidget* TopPrompt = UIBase ? UIBase->GetActivePromptWidget() : nullptr;
		return SaveMgr && !(TopPrompt && TopPrompt->IsA<UHQLevelUpCelebrationWidget>())
			&& FTutorialDerivedMissionRules::IsHQLevelUpDone(SaveMgr->GetHQLevel(), Amount, DerivedHQLevelSignalAt, Now);
	}
	case EMissionConditionType::MenuTour:
	{
		UUIManagerSubsystem* UIMgr = GI ? GI->GetSubsystem<UUIManagerSubsystem>() : nullptr;
		UUIBase* UIBase = UIMgr ? UIMgr->GetUIBase() : nullptr;
		UCommonActivatableWidget* TopPrompt = UIBase ? UIBase->GetActivePromptWidget() : nullptr;
		return FTutorialDerivedMissionRules::IsMenuTourDone(
			HasDerivedLatch(TutorialDerivedLatch::MenuIntroDone),
			TopPrompt != nullptr);
	}
	default:
		return false;
	}
}

bool UMissionManagerSubsystem::IsDerivedShortfallStep(const FGoalGuideStep& Step) const
{
	if (Step.Spot || !FTutorialDerivedMissionRules::IsPurchaseStep(ActiveMissionRow.ConditionType, Step.Index))
	{
		return false;
	}
	if (!Step.Anchor)
	{
		return true;
	}
	// 레이아웃 전 1프레임(ZeroSize)은 부족이 아니다 — 꺼진 버튼만 부족으로 읽는다
	FString Reason;
	return !IsGuideTargetActionable(Step.Anchor, Reason) && Reason == TEXT("Disabled");
}

void UMissionManagerSubsystem::SeedDerivedJumpPrereqs(FName TargetMissionID)
{
#if !UE_BUILD_SHIPPING
	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	bool bTargetFound = false;
	const FMissionTable TargetRow = TableMgr ? TableMgr->GetMissionData(TargetMissionID, bTargetFound) : FMissionTable();
	if (!bTargetFound || !TargetRow.bDerivedGuide)
	{
		return;
	}

	// 대상 앞 체인 행들의 보상 합 = 정상 플레이라면 이미 받았을 몫(M7 수령분). 모자란 만큼만 채운다
	TMap<EResourceType, int64> RequiredResources;
	TMap<EItemType, int32> RequiredItems;
	// 대상 미션이 다루는 회사(두 번째 회사 직원 채우기 등)까지는 지어져 있어야 한다
	int32 RequiredBuildings = FMath::Max(1, TargetRow.TutorialCompanyOrder);
	int32 RequiredHQLevel = 1;
	int32 FirstLaunchProject = 0;
	TSet<FName> VisitedIDs;
	FName ChainID = OpeningChainStartID;
	while (!ChainID.IsNone() && ChainID != TargetMissionID && !VisitedIDs.Contains(ChainID))
	{
		VisitedIDs.Add(ChainID);
		bool bFound = false;
		const FMissionTable Row = TableMgr->GetMissionData(ChainID, bFound);
		if (!bFound)
		{
			break;
		}
		for (const FMissionReward& Reward : Row.Rewards)
		{
			if (Reward.ResourceType != EResourceType::None && Reward.Amount > 0)
			{
				RequiredResources.FindOrAdd(Reward.ResourceType) += Reward.Amount;
			}
			if (Reward.ItemType != EItemType::None && Reward.ItemAmount > 0)
			{
				RequiredItems.FindOrAdd(Reward.ItemType) += Reward.ItemAmount;
			}
		}
		if (Row.ConditionType == EMissionConditionType::BuildSecondBuilding)
		{
			RequiredBuildings = FMath::Max(RequiredBuildings, FTutorialDerivedMissionRules::ResolveTarget(Row.ConditionAmount));
		}
		// 산업 지정 건설은 기존 회사에 더해 짓는 것이라 개수가 늘어난다
		if (Row.ConditionType == EMissionConditionType::BuildIndustry)
		{
			RequiredBuildings += FTutorialDerivedMissionRules::ResolveTarget(Row.ConditionAmount);
		}
		if (Row.ConditionType == EMissionConditionType::ReachHQLevel)
		{
			RequiredHQLevel = FMath::Max(RequiredHQLevel, FTutorialDerivedMissionRules::ResolveTarget(Row.ConditionAmount));
		}
		if (Row.ConditionType == EMissionConditionType::LaunchFirstInHouseProject)
		{
			FirstLaunchProject = Row.RequiredProjectIndex;
		}
		ChainID = Row.NextMissionID;
	}

	if (UResourceItemManager* Res = GI->GetSubsystem<UResourceItemManager>())
	{
		for (const TPair<EResourceType, int64>& Required : RequiredResources)
		{
			const int64 Owned = Res->GetResourceAmount(Required.Key);
			if (Owned < Required.Value)
			{
				Res->StoreResource(Required.Key, Required.Value - Owned);
				UE_LOG(LogTemp, Log, TEXT("[MissionManager] 시드: 자원 %d 을 %lld 보충 (도출형 점프 %s)"),
					static_cast<int32>(Required.Key), Required.Value - Owned, *TargetMissionID.ToString());
			}
		}
	}
	if (UItemInventoryManager* Items = GI->GetSubsystem<UItemInventoryManager>())
	{
		for (const TPair<EItemType, int32>& Required : RequiredItems)
		{
			const int32 Owned = Items->GetItemCount(Required.Key);
			if (Owned < Required.Value)
			{
				Items->AddItem(Required.Key, Required.Value - Owned);
				UE_LOG(LogTemp, Log, TEXT("[MissionManager] 시드: 아이템 %d 을 %d 보충 (도출형 점프 %s)"),
					static_cast<int32>(Required.Key), Required.Value - Owned, *TargetMissionID.ToString());
			}
		}
	}

	USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>();
	const USaveGame_GameData* Save = SaveMgr ? SaveMgr->GetCurrentSaveData() : nullptr;
	const int32 BuildingCount = Save ? Save->GameData.Buildings.Num() : 0;
	if (BuildingCount < RequiredBuildings)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[MissionManager] 시드: '%s'는 건물 %d채가 선행 필요(현재 %d채). 자동 건설은 하지 않으니 직접 지은 뒤 점프할 것."),
			*TargetMissionID.ToString(), RequiredBuildings, BuildingCount);
	}
	const int32 CurrentHQLevel = SaveMgr ? SaveMgr->GetHQLevel() : 1;
	if (CurrentHQLevel < RequiredHQLevel)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[MissionManager] 시드: '%s'는 본사 Lv.%d 가 선행 필요(현재 Lv.%d). 자동 레벨업은 하지 않으니 본사 패널에서 직접 올린 뒤 점프할 것."),
			*TargetMissionID.ToString(), RequiredHQLevel, CurrentHQLevel);
	}
	const FBuildingEntitySaveData* Company = FindTutorialCompanySave(GetTutorialCompanyIndex());
	if (FirstLaunchProject > 0 && (!Save || !Company || !FTutorialMissionRestoreRules::HasRequiredLaunch(
		Save->GameData.ShippedProjects, Company->BuildingIndex, Company->BuildingData.CompanyType, FirstLaunchProject)))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[MissionManager] 시드: 첫 게임(프로젝트 %d) 출시 이력이 없습니다. 사무실에서 Proj_Seed 치트로 이력을 채운 뒤 진행할 것."),
			FirstLaunchProject);
	}
#else
	(void)TargetMissionID;
#endif
}

namespace CGRGoalGuideStep
{
bool CanGuideDeskGrowth(const AWorkstationActorBase* Desk, bool bPromotion)
{
	if (!IsValid(Desk)) return false;
	if (bPromotion) return Desk->CanPromoteSetup();
	for (EWorkstationUpgradeType Type : { EWorkstationUpgradeType::Processing, EWorkstationUpgradeType::Tools,
		EWorkstationUpgradeType::Environment })
	{
		FWorkstationUpgradeQuote Quote;
		if (Desk->GetUpgradeQuote(Type, 1, false, Quote) && Quote.bValid
			&& Quote.Count > 0 && Quote.TotalCost <= Quote.Available) return true;
	}
	return false;
}

// 현재 직원이 레벨업할 수 없어도 안내는 직원 선택 단계에 머문다.
int32 ResolveEmployeeGrowthStep(bool bInTargetOffice, bool bWindowOpen, bool bStarModalOpen, bool bStarGoal)
{
	if (!bInTargetOffice) return 0;
	if (bStarGoal && bStarModalOpen) return 3;
	if (!bWindowOpen) return 1;
	return 2;
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
	const EGoalState GoalState = GoalMgr->GetGoalState(TrackedID, Row);
	if (GoalState != EGoalState::InProgress)
	{
		return Step;
	}

	FContentGuideSpec Spec;
	Spec.Condition = Row.ConditionType;
	Spec.ConditionAmount = Row.ConditionAmount;
	Spec.BuildingIndex = GoalMgr->GetGuideBuildingIndex(TrackedID);
	Spec.Discipline = Row.RequiredDiscipline;
	Spec.TargetProjectIndex = Row.TargetProjectIndex;
	Spec.Industry = Row.CompanyType;
	return ResolveContentStep(Spec);
}

namespace
{
	CGRGoalGuideStep::EGuidePromptKind ClassifyTutorialGuidePrompt(const UCommonActivatableWidget* Prompt)
	{
		using CGRGoalGuideStep::EGuidePromptKind;
		if (!Prompt) return EGuidePromptKind::None;
		if (Prompt->IsA<UBuildingTraitGachaPanelWidget>()) return EGuidePromptKind::CompanyGacha;
		if (Prompt->IsA<UEquipmentGachaPanelWidget>()) return EGuidePromptKind::EquipmentGacha;
		if (Prompt->IsA<UBuildModalWidget>()) return EGuidePromptKind::BuildModal;
		if (Prompt->IsA<UCityCompanyInfoWidget>()) return EGuidePromptKind::CompanyInfo;
		if (Prompt->IsA<UConfirmCancelWidget>()) return EGuidePromptKind::ConfirmCancel;
		if (Prompt->IsA<UHQManagePanelWidget>()) return EGuidePromptKind::HQPanel;
		if (Prompt->IsA<UHQLevelUpCelebrationWidget>()) return EGuidePromptKind::HQLevelUpCelebration;
		if (Prompt->IsA<UMenuPanelWidget>()) return EGuidePromptKind::MenuPanel;
		if (Prompt->IsA<UShopPanelWidget>()) return EGuidePromptKind::ShopPanel;
		if (Prompt->IsA<UEnhanceStarforceModalWidget>()) return EGuidePromptKind::StarforceModal;
		return EGuidePromptKind::Other;
	}

	// 닫기 게터가 있는 창만 짚는다 — 없으면 대상 없이 풀어 두어 플레이어가 직접 닫게 한다(가두지 않는 쪽)
	UWidget* FindTutorialPanelCloseButton(const UCommonActivatableWidget* Panel)
	{
		if (const UGachaPanelBaseWidget* Gacha = Cast<UGachaPanelBaseWidget>(Panel)) return Gacha->GetCloseButtonWidget();
		if (const UEquipmentGachaPanelWidget* EquipGacha = Cast<UEquipmentGachaPanelWidget>(Panel)) return EquipGacha->GetCloseButtonWidget();
		if (const UMenuPanelWidget* Menu = Cast<UMenuPanelWidget>(Panel)) return Menu->GetCloseButtonWidget();
		if (const UShopPanelWidget* Shop = Cast<UShopPanelWidget>(Panel)) return Shop->GetCloseButtonWidget();
		if (const UWorkstationInfoWidget* DeskInfo = Cast<UWorkstationInfoWidget>(Panel)) return DeskInfo->GetCloseButtonWidget();
		if (const UBuildingManagePanelWidget* CompanyPanel = Cast<UBuildingManagePanelWidget>(Panel)) return CompanyPanel->GetCloseButtonWidget();
		if (const UHQManagePanelWidget* HQPanel = Cast<UHQManagePanelWidget>(Panel)) return HQPanel->GetCloseButtonWidget();
		if (const UHQLevelUpCelebrationWidget* Celebration = Cast<UHQLevelUpCelebrationWidget>(Panel)) return Celebration->GetCloseTargetWidget();
		if (const UFactoryPanelWidget* Factory = Cast<UFactoryPanelWidget>(Panel)) return Factory->GetCloseButtonWidget();
		if (const UEnhanceStarforceModalWidget* Starforce = Cast<UEnhanceStarforceModalWidget>(Panel)) return Starforce->GetCloseButtonWidget();
		if (const UProjectReportWidget* Report = Cast<UProjectReportWidget>(Panel)) return Report->GetCloseTargetWidget();
		if (const UCityCompanyManageWidget* AcquiredPanel = Cast<UCityCompanyManageWidget>(Panel)) return AcquiredPanel->GetCloseButtonWidget();
		if (const UPlotPurchasePanelWidget* PlotPanel = Cast<UPlotPurchasePanelWidget>(Panel)) return PlotPanel->GetCloseButtonWidget();
		if (const UOfficeEmployeePanelWidget* EmployeePanel = Cast<UOfficeEmployeePanelWidget>(Panel)) return EmployeePanel->GetCloseButtonWidget();
		if (const UOfficeDecorationPanelWidget* DecorationPanel = Cast<UOfficeDecorationPanelWidget>(Panel)) return DecorationPanel->GetCloseButtonWidget();
		if (const UOfficeWorkstationPanelWidget* DeskCatalog = Cast<UOfficeWorkstationPanelWidget>(Panel)) return DeskCatalog->GetCloseButtonWidget();
		return nullptr;
	}

	// 아래쪽 스택 패널이 이 미션 흐름의 것인가 — 앞 미션에서 열어 둔 패널이 다음 대상을 덮으면 먼저 닫게 한다
	bool IsTutorialBottomPanelInFlow(EMissionConditionType Condition, const UCommonActivatableWidget* Bottom)
	{
		if (!Bottom) return true;
		if (Bottom->IsA<UFactoryPanelWidget>()) return Condition == EMissionConditionType::UpgradeFactoryStats;
		if (Bottom->IsA<UCityCompanyManageWidget>())
			return Condition == EMissionConditionType::SkipCompanyRecovery || Condition == EMissionConditionType::DemolishFirstCompany;
		if (Bottom->IsA<UPlotPurchasePanelWidget>()) return Condition == EMissionConditionType::ExplainPlot;
		if (Bottom->IsA<UOfficeEmployeePanelWidget>())
			return Condition == EMissionConditionType::LevelUpFirstEmployee || Condition == EMissionConditionType::AllocateEmployeeStatPoint
				|| Condition == EMissionConditionType::EnhanceFirstEmployee || Condition == EMissionConditionType::EquipFirstEquipment;
		if (Bottom->IsA<UOfficeDecorationPanelWidget>()) return Condition == EMissionConditionType::PlaceFirstPainting;
		if (Bottom->IsA<UOfficeWorkstationPanelWidget>())
			return Condition == EMissionConditionType::PlaceSecondDesk || Condition == EMissionConditionType::RecruitGoalDiscipline;
		return true;
	}


	// 연출은 닫힐 때 부모에서 떨어진다 — 약참조가 GC 전까지 남아도 화면에 없으면 닫힌 것
	bool IsTutorialGachaRevealShown(const UGachaRevealPresentationWidget* Reveal)
	{
		return Reveal && (Reveal->IsInViewport() || Reveal->GetParent() != nullptr);
	}

	// 살 수 없는 구매 버튼은 짚지 않는다 — 대상이 비면 부족 문구로 바뀌고 게이트가 풀려 재화를 모으러 갈 수 있다
	UWidget* KeepTutorialAnchorIfAffordable(UWidget* Button)
	{
		const UCostActionButtonWidget* CostButton = Cast<UCostActionButtonWidget>(Button);
		return CostButton && !CostButton->CanAfford() ? nullptr : Button;
	}

	// 직원 패널의 [장비 뽑기]는 보유 0개 부위의 빈 칸을 눌렀을 때만 뜬다 — 그런 부위가 있는지와 끼지 않은 장비가 있는지
	void ResolveTutorialEquipmentStock(const UGameInstance* GI, bool& bOutOwnsUnequipped, bool& bOutPanelGachaReachable)
	{
		bOutOwnsUnequipped = false;
		bOutPanelGachaReachable = false;
		const UEmployeeEquipmentManagerSubsystem* Equipment = GI ? GI->GetSubsystem<UEmployeeEquipmentManagerSubsystem>() : nullptr;
		const UTableManagerSubsystem* Tables = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
		if (!Equipment || !Tables)
		{
			return;
		}
		TArray<int32> OwnedPerPart;
		OwnedPerPart.Init(0, static_cast<int32>(EEmployeeEquipmentSlot::Count));
		for (const TPair<FGuid, FEmployeeEquipmentInstance>& Pair : Equipment->GetState().Instances)
		{
			if (Equipment->GetInstanceEmployee(Pair.Key) == INDEX_NONE)
			{
				bOutOwnsUnequipped = true;
			}
			const FEmployeeEquipmentDefinitionRow* Definition = Tables->FindEmployeeEquipmentDefinition(Pair.Value.DefinitionId);
			const int32 PartIndex = Definition ? static_cast<int32>(Definition->EquipmentSlot) : INDEX_NONE;
			if (OwnedPerPart.IsValidIndex(PartIndex))
			{
				++OwnedPerPart[PartIndex];
			}
		}
		bOutPanelGachaReachable = OwnedPerPart.Contains(0);
	}
}

bool UMissionManagerSubsystem::ResolveForcedTutorialStep(const FContentGuideSpec& Spec, FGoalGuideStep& Step) const
{
	using namespace CGRGoalGuideStep;

	UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance());
	if (!GI)
	{
		return false;
	}
	UWorld* CurWorld = GetWorld();
	const bool bInOffice = GI->IsInOfficeMap();
	const bool bInTargetOffice = bInOffice
		&& (Spec.BuildingIndex == INDEX_NONE || GI->GetCurrentManagedBuildingIndex() == Spec.BuildingIndex);
	UUIManagerSubsystem* UIMgr = GI->GetSubsystem<UUIManagerSubsystem>();
	UUIBase* UIBase = UIMgr ? UIMgr->GetUIBase() : nullptr;
	UCommonActivatableWidget* TopPrompt = UIBase ? UIBase->GetActivePromptWidget() : nullptr;
	UCommonActivatableWidget* TopBottom = UIBase ? UIBase->GetActiveBottomWidget() : nullptr;
	UEntityManager* Entities = CurWorld ? CurWorld->GetSubsystem<UEntityManager>() : nullptr;
	ABuildingBaseActor* Company = Entities && Spec.BuildingIndex != INDEX_NONE
		? Entities->GetBuildingByIndex(Spec.BuildingIndex) : nullptr;
	UBuildingManagePanelWidget* Manage = ManagePanelWeak.Get();
	if (Manage && !Manage->IsActivated()) Manage = nullptr;
	const bool bCompanyPanel = Manage && Company && Manage->GetTargetBuilding() == Company;
	UBuildingManagePanelWidget* OtherPanel = Manage && !bCompanyPanel ? Manage : nullptr;
	// 이 걸음의 대상이 다른 창에 가려 그 창의 닫기를 짚을 때는 걸음 문구 대신 「먼저 이 창을 닫아요」를 쓴다
	auto CloseFirst = [&Step](UWidget* Close) { Step.Index = INDEX_NONE; return Close; };
	// 도크는 아래쪽 패널에 덮이면 비활성되며 등록이 풀린다 — 도크 버튼 걸음은 덮은 패널 [닫기]로 대신한다
	UBuildOpenWidget* Dock = BuildOpenWidgetWeak.Get();
	// 바닥 스택은 맨 위 하나만 그린다 — 덮인 도크 버튼은 옛 지오메트리만 남아 게이트 구멍이 허공에 뚫린다
	UOfficeMainWidget* OfficeDock = OfficeMainWeak.Get();
	if (OfficeDock && TopBottom != OfficeDock) OfficeDock = nullptr;
	UCityAcquisitionManager* AcqMgr = CurWorld ? CurWorld->GetSubsystem<UCityAcquisitionManager>() : nullptr;
	const int32 Target = FTutorialDerivedMissionRules::ResolveTarget(Spec.ConditionAmount);

	if (IsRewardSplashShown())
	{
		Step.Index = INDEX_NONE;
		return true;
	}

	// 운영 결산 창은 어느 미션 흐름에도 속하지 않는다 — 운영이 끝나면 아무 걸음 위에나 뜨므로 [확인]부터
	if (const UProjectReportWidget* Report = Cast<UProjectReportWidget>(TopPrompt))
	{
		Step.Index = INDEX_NONE;
		Step.Anchor = VisibleOrNull(Report->GetCloseTargetWidget());
		return true;
	}
	// 흐름 밖 창이 떠 있으면 그 창부터 — 닫기 게터가 없으면 대상 없이 풀어 두고, 번호는 비워 직전 걸음 문구를 유지한다
	if (!IsPromptInFlow(Spec.Condition, ClassifyTutorialGuidePrompt(TopPrompt)))
	{
		Step.Index = INDEX_NONE;
		Step.Anchor = FindTutorialPanelCloseButton(TopPrompt);
		return true;
	}
	if (!IsTutorialBottomPanelInFlow(Spec.Condition, TopBottom))
	{
		if (UWidget* BottomClose = FindTutorialPanelCloseButton(TopBottom))
		{
			Step.Index = INDEX_NONE;
			Step.Anchor = BottomClose;
			return true;
		}
	}

	auto ExitOfficeAt = [this, &Step](int32 Index)
	{
		Step.Index = Index;
		Step.Anchor = OfficeLayerWeak.IsValid() ? OfficeLayerWeak->GetBackButtonWidget() : nullptr;
	};
	// 사무실 걸음인데 다른 곳이면: 다른 사무실은 [나가기], 도시는 다른 회사 패널 닫기 → 첫 회사 패널 [입장] → 첫 회사 지목
	auto MoveToTargetOffice = [&Step, &ExitOfficeAt, &CloseFirst, bInOffice, OtherPanel, bCompanyPanel, Manage, Company](int32 Index)
	{
		if (bInOffice)
		{
			ExitOfficeAt(Index);
			return;
		}
		Step.Index = Index;
		if (OtherPanel) Step.Anchor = CloseFirst(OtherPanel->GetCloseButtonWidget());
		else if (bCompanyPanel) Step.Anchor = Manage->GetEnterOfficeButtonWidget();
		else Step.Spot = Company;
	};
	// 같은 종류지만 이 걸음의 대상이 아닌 창(다른 회사 인수 창·다른 확인 창) — 대상 없이 풀어 둔다
	auto ReleaseForUnrelatedPrompt = [&Step]()
	{
		Step.Index = INDEX_NONE;
		Step.Anchor = nullptr;
		Step.Spot = nullptr;
	};

	switch (Spec.Condition)
	{
	case EMissionConditionType::UpgradeFactoryStats:
	{
		if (!BrickFactoryWeak.IsValid() && CurWorld && !bInOffice)
		{
			BrickFactoryWeak = UGameplayStatics::GetActorOfClass(CurWorld, ABrickFactory::StaticClass());
		}
		const ABrickFactory* Factory = Cast<ABrickFactory>(BrickFactoryWeak.Get());
		UFactoryPanelWidget* FactoryPanel = FactoryPanelWeak.Get();
		const bool bSpeedReached = Factory && Factory->GetUpgradeLevel(EFactoryUpgradeType::HoldProductionSpeed) >= Target;
		const bool bCapacityReached = Factory && Factory->GetUpgradeLevel(EFactoryUpgradeType::HoldProductionAmount) >= Target;
		Step.Index = ResolveFactoryStatStep(bInOffice, FactoryPanel != nullptr, bSpeedReached, bCapacityReached);
		if (Step.Index == FactoryStatGuide::ExitOffice)
		{
			ExitOfficeAt(FactoryStatGuide::ExitOffice);
		}
		else if (Step.Index == FactoryStatGuide::ClosePanel)
		{
			Step.Anchor = FactoryPanel ? FactoryPanel->GetCloseButtonWidget() : nullptr;
		}
		else if (Step.Index == FactoryStatGuide::OpenFactory)
		{
			Step.Anchor = Dock ? Dock->GetFactoryOpenButtonWidget() : CloseFirst(FindTutorialPanelCloseButton(TopBottom));
		}
		else
		{
			Step.Anchor = KeepTutorialAnchorIfAffordable(FactoryPanel->GetUpgradeTarget(Step.Index == FactoryStatGuide::UpgradeSpeed
				? EFactoryUpgradeType::HoldProductionSpeed : EFactoryUpgradeType::HoldProductionAmount));
		}
		return true;
	}

	case EMissionConditionType::RaiseBuildingFloor:
	{
		const bool bFloorReached = Company && Company->GetEnhancementLevel(EBuildingEnhancementType::BuildingFloor) >= Target;
		Step.Index = ResolveForcedRaiseFloorStep(bInOffice, OtherPanel != nullptr, bCompanyPanel,
			bCompanyPanel && Manage->IsEnhancementTabActive(), bFloorReached);
		switch (Step.Index)
		{
		case RaiseFloorGuide::ExitOffice:      ExitOfficeAt(RaiseFloorGuide::ExitOffice); break;
		case RaiseFloorGuide::CloseOtherPanel: Step.Anchor = OtherPanel->GetCloseButtonWidget(); break;
		// 층은 패널 뒤 건물에서 오른다 — 잠시 지켜본 뒤 같은 패널에서 다음 미션(자금 강화)으로 이어진다
		case RaiseFloorGuide::WatchFloorRise:  break;
		case RaiseFloorGuide::TapCompany:      Step.Spot = Company; break;
		case RaiseFloorGuide::OpenEnhanceTab:  Step.Anchor = Manage->GetEnhancementTabButtonWidget(); break;
		default:                               Step.Anchor = KeepTutorialAnchorIfAffordable(Manage->GetBuildingFloorUpgradeButtonWidget()); break;
		}
		return true;
	}

	case EMissionConditionType::EnhanceBuildingStat:
		Step.Index = ResolveEnhanceStatStep(bInOffice, bCompanyPanel, bCompanyPanel && Manage->IsEnhancementTabActive());
		if (Step.Index == EnhanceStatGuide::ExitOffice)
		{
			ExitOfficeAt(EnhanceStatGuide::ExitOffice);
		}
		else if (Step.Index == EnhanceStatGuide::TapCompany)
		{
			if (OtherPanel) Step.Anchor = CloseFirst(OtherPanel->GetCloseButtonWidget());
			else Step.Spot = Company;
		}
		else if (Step.Index == EnhanceStatGuide::OpenEnhanceTab)
		{
			Step.Anchor = Manage->GetEnhancementTabButtonWidget();
		}
		else
		{
			Step.Anchor = Manage->GetFirstAffordableStatEnhanceButtonWidget();
		}
		return true;

	case EMissionConditionType::EquipFirstTrait:
	case EMissionConditionType::EquipFirstSkin:
	{
		const bool bTrait = Spec.Condition == EMissionConditionType::EquipFirstTrait;
		UBuildingTraitGachaPanelWidget* Shell = Cast<UBuildingTraitGachaPanelWidget>(TopPrompt);
		UGachaRevealPresentationWidget* Reveal = GachaRevealWeak.Get();
		bool bOwnsResult = false;
		if (bTrait)
		{
			// 이 회사에 바로 끼울 수 있는 특성이어야 뽑기 걸음이 끝난 것(업종 제한·이미 낀 특성 제외)
			const UBuildingTraitManagerSubsystem* TraitMgr = GI->GetSubsystem<UBuildingTraitManagerSubsystem>();
			if (TraitMgr && Spec.BuildingIndex != INDEX_NONE)
			{
				const TArray<FName> EquippedHere = TraitMgr->GetEquippedTraits(Spec.BuildingIndex);
				for (const TPair<FName, int32>& Owned : TraitMgr->GetInventory().OwnedTraits)
				{
					if (Owned.Value > 0 && !EquippedHere.Contains(Owned.Key)
						&& TraitMgr->CanEquipTraitToBuilding(Spec.BuildingIndex, Owned.Key))
					{
						bOwnsResult = true;
						break;
					}
				}
			}
		}
		else
		{
			const UBuildingSkinManagerSubsystem* SkinMgr = GI->GetSubsystem<UBuildingSkinManagerSubsystem>();
			bOwnsResult = SkinMgr && SkinMgr->HasAnyGachaSkin();
		}

		FCompanyGachaGuideFlags Flags;
		Flags.bInOffice = bInOffice;
		Flags.bRevealOpen = IsTutorialGachaRevealShown(Reveal);
		Flags.bGachaOpen = Shell != nullptr;
		Flags.bWrongCategory = Shell && Shell->IsSkinCategory() == bTrait;
		Flags.bOwnsResult = bOwnsResult;
		Flags.bCompanyPanelOpen = bCompanyPanel;
		Flags.bTargetTabActive = bCompanyPanel && (bTrait ? Manage->IsTraitTabActive() : Manage->IsSkinTabActive());
		Step.Index = ResolveCompanyGachaStep(Flags);
		switch (Step.Index)
		{
		case TraitGuide::ExitOffice:    ExitOfficeAt(TraitGuide::ExitOffice); break;
		case TraitGuide::OpenGacha:     Step.Anchor = Dock ? Dock->GetGachaButtonWidget() : CloseFirst(FindTutorialPanelCloseButton(TopBottom)); break;
		case TraitGuide::PickCategory:  Step.Anchor = bTrait ? Shell->GetTraitCategoryTabWidget() : Shell->GetSkinCategoryTabWidget(); break;
		// 고급 배너는 보석을 쓴다 — 받은 일반 뽑기권으로 뽑게 일반 배너부터
		case TraitGuide::PressPull:     Step.Anchor = Shell->IsAdvancedBannerSelected() ? Shell->GetNormalBannerWidget() : Shell->GetPullButtonWidget(); break;
		case TraitGuide::ConfirmReveal: Step.Anchor = VisibleOrNull(Reveal->GetConfirmButtonWidget()); break;
		case TraitGuide::CloseGacha:    Step.Anchor = Shell->GetCloseButtonWidget(); break;
		case TraitGuide::OpenTraitTab:
			if (OtherPanel) Step.Anchor = CloseFirst(OtherPanel->GetCloseButtonWidget());
			else if (bCompanyPanel) Step.Anchor = bTrait ? Manage->GetTraitTabButtonWidget() : Manage->GetSkinTabButtonWidget();
			else Step.Spot = Company;
			break;
		default:
			if (bTrait)
			{
				Step.Anchor = Manage->GetTraitEquipButtonIfOpen();
				if (!Step.Anchor) Step.Anchor = Manage->GetEquippableTraitCardWidget(TutorialPulledTraitID);
			}
			else
			{
				Step.Anchor = Manage->GetSkinExteriorSubTabWidget();
				if (!Step.Anchor) Step.Anchor = Manage->GetOwnedGachaSkinCardWidget();
			}
			break;
		}
		return true;
	}

	case EMissionConditionType::PlaceSecondDesk:
	case EMissionConditionType::PlaceFirstPainting:
	{
		const bool bDesk = Spec.Condition == EMissionConditionType::PlaceSecondDesk;
		const EPlacementMode Mode = GetTutorialPlacementMode(GI);
		UOfficeWorkstationPanelWidget* DeskCatalog = bDesk ? Cast<UOfficeWorkstationPanelWidget>(TopBottom) : nullptr;
		UOfficeDecorationPanelWidget* DecoCatalog = bDesk ? nullptr : Cast<UOfficeDecorationPanelWidget>(TopBottom);
		Step.Index = ResolveOfficePlacementStep(bInTargetOffice, DeskCatalog != nullptr || DecoCatalog != nullptr,
			Mode == (bDesk ? EPlacementMode::Workstation : EPlacementMode::WallDecoration));
		if (Step.Index == SecondDeskGuide::EnterOffice)
		{
			MoveToTargetOffice(SecondDeskGuide::EnterOffice);
		}
		else if (Step.Index == SecondDeskGuide::Confirm)
		{
			Step.Anchor = BuildPlacementPanelWeak.IsValid() ? BuildPlacementPanelWeak->GetPlaceButtonWidget() : nullptr;
			// 여러 개를 놓는 미션은 같은 말을 되풀이하면 놓였는지 모른다 ― 마지막 하나는 「하나 더」로 말한다
			if (bDesk && Target >= 2 && CountPlacedSeats(CurWorld) == Target - 1)
			{
				Step.Index = SecondDeskGuide::ConfirmMore;
			}
		}
		else if (Step.Index == SecondDeskGuide::PickDesk)
		{
			if (DeskCatalog)
			{
				Step.Anchor = DeskCatalog->GetDeskGuideCatalogWidget();
			}
			else
			{
				Step.Anchor = DecoCatalog->IsCategoryListed(EDecorationCategory::Picture)
					? DecoCatalog->GetCheapestAffordableCardWidget(EDecorationCategory::Picture)
					: DecoCatalog->GetCategoryNavigationWidget(EDecorationCategory::Picture);
			}
		}
		else if (OfficeDock)
		{
			Step.Anchor = bDesk ? OfficeDock->GetWorkstationOpenButtonWidget() : OfficeDock->GetDecorationOpenButtonWidget();
		}
		else
		{
			Step.Anchor = CloseFirst(FindTutorialPanelCloseButton(TopBottom));
		}
		return true;
	}

	case EMissionConditionType::EquipFirstEquipment:
	{
		// 앞 미션의 별 강화 카드는 활성화하지 않는 오버레이라 패널 판정이 못 본다 ― 결과를 본 뒤 직접 닫게 한다
		if (UEnhanceStarforceModalWidget* StarCard = StarforceModalWeak.Get(); StarCard && StarCard->GetParent())
		{
			Step.Anchor = CloseFirst(StarCard->GetCloseButtonWidget());
			return true;
		}
		UOfficeEmployeePanelWidget* EmpPanel = Cast<UOfficeEmployeePanelWidget>(TopBottom);
		UEquipmentGachaPanelWidget* EquipGacha = Cast<UEquipmentGachaPanelWidget>(TopPrompt);
		UGachaRevealPresentationWidget* Reveal = GachaRevealWeak.Get();
		// 장비 결과 연출은 장비 작업 공간이 따로 띄운다 — 공용 등록이 없으면 그쪽을 본다
		if (!IsTutorialGachaRevealShown(Reveal) && EquipGacha)
		{
			Reveal = EquipGacha->GetActiveReveal();
		}
		FEquipmentGuideFlags Flags;
		ResolveTutorialEquipmentStock(GI, Flags.bOwnsUnequipped, Flags.bPanelGachaReachable);
		Flags.bInTargetOffice = bInTargetOffice;
		Flags.bRevealOpen = IsTutorialGachaRevealShown(Reveal);
		Flags.bGachaOpen = EquipGacha != nullptr;
		Flags.bEmployeePanelOpen = EmpPanel != nullptr;
		Flags.bEquipmentTabActive = EmpPanel && EmpPanel->IsEquipmentTabActive();
		Flags.bPanelGachaVisible = EmpPanel && EmpPanel->GetEquipmentGachaButtonWidget() != nullptr;
		Step.Index = ResolveEquipmentStep(Flags);
		switch (Step.Index)
		{
		case EquipmentGuide::EnterOffice:
			MoveToTargetOffice(EquipmentGuide::EnterOffice);
			break;
		case EquipmentGuide::OpenEmployee:
			Step.Anchor = OfficeDock ? OfficeDock->GetEmployeeButtonWidget() : CloseFirst(FindTutorialPanelCloseButton(TopBottom));
			break;
		case EquipmentGuide::PickSlot:
			Step.Anchor = Flags.bEquipmentTabActive ? EmpPanel->GetEmptyEquipmentSlotWidget() : EmpPanel->GetEquipmentTabButtonWidget();
			break;
		case EquipmentGuide::OpenGacha:
			// 패널 안 진입이 없으면 도크 [장비] — 패널이 덮고 있으면 도크가 안 보이므로 대상 없이 둔다
			if (EmpPanel) Step.Anchor = EmpPanel->GetEquipmentGachaButtonWidget();
			else Step.Anchor = OfficeDock ? OfficeDock->GetEquipmentGachaButtonWidget() : CloseFirst(FindTutorialPanelCloseButton(TopBottom));
			break;
		case EquipmentGuide::PressPull:
			Step.Anchor = EquipGacha->IsNormalTicketSelected() ? EquipGacha->GetPullOneButtonWidget() : EquipGacha->GetNormalTicketTabWidget();
			break;
		case EquipmentGuide::ConfirmReveal:
			Step.Anchor = VisibleOrNull(Reveal->GetConfirmButtonWidget());
			break;
		case EquipmentGuide::CloseGacha:
			Step.Anchor = EquipGacha->GetCloseButtonWidget();
			break;
		default:
			Step.Anchor = EmpPanel->GetEquipDetailEquipButtonWidget();
			if (!Step.Anchor) Step.Anchor = EmpPanel->GetUnequippedEquipmentEntryWidget();
			if (!Step.Anchor) Step.Anchor = EmpPanel->GetAllEquipmentPartsChipWidget();
			break;
		}
		return true;
	}

	case EMissionConditionType::BuildSecondBuilding:
	case EMissionConditionType::BuildIndustry:
	{
		UBuildModalWidget* Modal = Cast<UBuildModalWidget>(TopPrompt);
		// 모달은 첫 산업을 자동 선택한다 — 목표 산업을 직접 누르기 전이나 다른 산업이 골라져 있으면 카드로 넘어가지 않는다
		const bool bIndustryPending = Modal && Spec.Industry != ECompanyType::None
			&& (!bDerivedBuildIndustryPicked || Modal->GetSelectedIndustry() != Spec.Industry);
		Step.Index = ResolveSecondBuildingStep(bInOffice, Modal != nullptr, bIndustryPending,
			GetTutorialPlacementMode(GI) == EPlacementMode::Building);
		switch (Step.Index)
		{
		case SecondBuildingGuide::ExitOffice:   ExitOfficeAt(SecondBuildingGuide::ExitOffice); break;
		case SecondBuildingGuide::OpenBuild:    Step.Anchor = Dock ? Dock->GetBuildOpenButtonWidget() : CloseFirst(FindTutorialPanelCloseButton(TopBottom)); break;
		case SecondBuildingGuide::PickIndustry: Step.Anchor = Modal->GetIndustryTileWidget(Spec.Industry); break;
		case SecondBuildingGuide::PickBuilding: Step.Anchor = Modal->GetBuildGuideBestCardWidget(); break;
		default: Step.Anchor = BuildPlacementPanelWeak.IsValid() ? BuildPlacementPanelWeak->GetPlaceButtonWidget() : nullptr; break;
		}
		return true;
	}

	case EMissionConditionType::AcquireFirstCompany:
	{
		// 필수 튜토리얼에서만 가장 싼 회사를 지목한다(성장 도움말은 선택을 플레이어에게 맡긴다)
		const int32 TargetKey = AcqMgr ? AcqMgr->FindCheapestAcquirableCompanyKey() : INDEX_NONE;
		UCityCompanyInfoWidget* Info = Cast<UCityCompanyInfoWidget>(TopPrompt);
		if (Info && (TargetKey == INDEX_NONE || Info->GetCompanyKey() != TargetKey))
		{
			ReleaseForUnrelatedPrompt();
			return true;
		}
		Step.Index = ResolveAcquireStep(bInOffice, Info != nullptr);
		if (Step.Index == AcquireGuide::ExitOffice) ExitOfficeAt(AcquireGuide::ExitOffice);
		else if (Step.Index == AcquireGuide::PressAcquire) Step.Anchor = Info->GetAcquireButtonWidget();
		else Step.Spot = AcqMgr && TargetKey != INDEX_NONE ? AcqMgr->GetCompanyActor(TargetKey) : nullptr;
		return true;
	}

	case EMissionConditionType::SkipCompanyRecovery:
	{
		const int32 TargetKey = AcqMgr ? AcqMgr->FindFirstCompanyKeyInState(EAcqState::Milking) : INDEX_NONE;
		UCityCompanyInfoWidget* Info = Cast<UCityCompanyInfoWidget>(TopPrompt);
		UCityCompanyManageWidget* CompanyPanel = Cast<UCityCompanyManageWidget>(TopBottom);
		if (CompanyPanel && (TargetKey == INDEX_NONE || CompanyPanel->GetCompanyKey() != TargetKey)) CompanyPanel = nullptr;
		UWidget* SkipConfirm = CompanyPanel ? CompanyPanel->GetSkipConfirmButtonWidget() : nullptr;
		// 인수 직후 결과 창이 남아 있다 — 그 [확인]이 관리 창을 연다
		const bool bResultPending = Info && Info->IsAwaitingResultConfirm();
		if (TopPrompt && !bResultPending && !SkipConfirm)
		{
			ReleaseForUnrelatedPrompt();
			return true;
		}
		Step.Index = ResolveSkipRecoveryStep(bInOffice, bResultPending, CompanyPanel != nullptr, SkipConfirm != nullptr);
		switch (Step.Index)
		{
		case SkipRecoveryGuide::ExitOffice: ExitOfficeAt(SkipRecoveryGuide::ExitOffice); break;
		case SkipRecoveryGuide::PressSkip:  Step.Anchor = CompanyPanel->GetSkipRecoveryButtonWidget(); break;
		case SkipRecoveryGuide::Confirm:    Step.Anchor = SkipConfirm; break;
		case SkipRecoveryGuide::ConfirmResult: Step.Anchor = Info->GetAcquireButtonWidget(); break;
		default: Step.Spot = AcqMgr && TargetKey != INDEX_NONE ? AcqMgr->GetCompanyActor(TargetKey) : nullptr; break;
		}
		return true;
	}

	case EMissionConditionType::DemolishFirstCompany:
	{
		int32 TargetKey = AcqMgr ? AcqMgr->FindFirstCompanyKeyInState(EAcqState::Depleted) : INDEX_NONE;
		if (TargetKey == INDEX_NONE && AcqMgr)
		{
			TargetKey = AcqMgr->FindFirstCompanyKeyInState(EAcqState::Milking);
		}
		const bool bDemolished = AcqMgr && AcqMgr->FindFirstCompanyKeyInState(EAcqState::Cleared) != INDEX_NONE;
		UCityCompanyManageWidget* CompanyPanel = Cast<UCityCompanyManageWidget>(TopBottom);
		if (CompanyPanel && (TargetKey == INDEX_NONE || CompanyPanel->GetCompanyKey() != TargetKey)) CompanyPanel = nullptr;
		// 다 회수한 회사는 확인 없이 바로 철거된다 — 확인 창은 회수 중 철거에만 뜬다
		UWidget* DemolishConfirm = CompanyPanel ? CompanyPanel->GetDemolishConfirmButtonWidget() : nullptr;
		if (TopPrompt && !DemolishConfirm)
		{
			ReleaseForUnrelatedPrompt();
			return true;
		}
		Step.Index = ResolveDemolishStep(bInOffice, bDemolished, CompanyPanel != nullptr, DemolishConfirm != nullptr);
		switch (Step.Index)
		{
		case DemolishGuide::ExitOffice:    ExitOfficeAt(DemolishGuide::ExitOffice); break;
		case DemolishGuide::Confirm:       Step.Anchor = DemolishConfirm; break;
		case DemolishGuide::PressDemolish: Step.Anchor = CompanyPanel->GetDemolishButtonWidget(); break;
		case DemolishGuide::TapCompany:    Step.Spot = AcqMgr && TargetKey != INDEX_NONE ? AcqMgr->GetCompanyActor(TargetKey) : nullptr; break;
		default:
		{
			// 회수금은 철거 순간 지갑에 들어온다 ― 무너지는 건물 옆에서 그 이유를 말한다
			const int32 ClearedKey = AcqMgr ? AcqMgr->FindFirstCompanyKeyInState(EAcqState::Cleared) : INDEX_NONE;
			Step.Spot = AcqMgr && ClearedKey != INDEX_NONE ? AcqMgr->GetCompanyActor(ClearedKey) : nullptr;
			Step.bObserve = Step.Spot != nullptr;
			break;
		}
		}
		return true;
	}

	case EMissionConditionType::ExplainPlot:
	{
		const int32 ClearedKey = AcqMgr ? AcqMgr->FindFirstCompanyKeyInState(EAcqState::Cleared) : INDEX_NONE;
		ACityPlotActor* Plot = FindTutorialClearedPlot(CurWorld);
		const bool bDialogOpen = Plot && Plot->IsPurchaseDialogOpen();
		if (TopPrompt && !bDialogOpen)
		{
			ReleaseForUnrelatedPrompt();
			return true;
		}
		Step.Index = ResolvePlotExplainStep(bInOffice, bDialogOpen, HasDerivedLatch(TutorialDerivedLatch::PlotDialogSeen));
		if (Step.Index == PlotExplainGuide::ExitOffice)
		{
			ExitOfficeAt(PlotExplainGuide::ExitOffice);
		}
		else if (Step.Index == PlotExplainGuide::Explain)
		{
			// 설명은 잠긴 [인수] 옆에서 한다 ― [닫기]에 붙이면 닫으라는 말로 읽힌다
			Step.Anchor = Plot->GetPurchaseDialogConfirmButtonWidget();
			Step.bObserve = true;
		}
		else if (Step.Index == PlotExplainGuide::CloseDialog)
		{
			Step.Anchor = Plot->GetPurchaseDialogCloseButtonWidget();
		}
		else
		{
			// 철거된 건물은 숨겨져도 자리는 남는다 — 그 자리를 짚어야 다른 입주 회사 클릭 영역과 겹치지 않는다
			AActor* ClearedActor = AcqMgr ? AcqMgr->GetCompanyActor(ClearedKey) : nullptr;
			Step.Spot = ClearedActor ? ClearedActor : Plot;
		}
		return true;
	}

	case EMissionConditionType::ReachHQLevel:
	{
		UHQManagePanelWidget* HQ = Cast<UHQManagePanelWidget>(TopPrompt);
		USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>();
		const bool bLevelReached = SaveMgr && SaveMgr->GetHQLevel() >= Target;
		Step.Index = ResolveHQLevelUpStep(bInOffice, HQ != nullptr, HQ && HQ->IsGrowthTabActive(),
			HasDerivedLatch(TutorialDerivedLatch::HQIntroDone), bLevelReached);
		switch (Step.Index)
		{
		case HQLevelUpGuide::ExitOffice:    ExitOfficeAt(HQLevelUpGuide::ExitOffice); break;
		case HQLevelUpGuide::OpenHQ:        Step.Anchor = Dock ? Dock->GetHeadquartersButtonWidget() : CloseFirst(FindTutorialPanelCloseButton(TopBottom)); break;
		// 성장 탭이 아니면 탭, 성장 탭이면 [레벨업] — 조건 미달로 꺼져 있으면 게이트가 풀리고 부족 문구가 뜬다
		case HQLevelUpGuide::OpenGrowthTab:
		case HQLevelUpGuide::PressLevelUp:  Step.Anchor = HQ->GetGrowthGuideTargetWidget(); break;
		case HQLevelUpGuide::WatchLevelUp:  Step.Anchor = Cast<UHQLevelUpCelebrationWidget>(TopPrompt) ? FindTutorialPanelCloseButton(TopPrompt) : nullptr; break;
		default: break; // 설명은 코치마크가 진행한다
		}
		return true;
	}

	case EMissionConditionType::MenuTour:
	{
		UMenuPanelWidget* Menu = Cast<UMenuPanelWidget>(TopPrompt);
		Step.Index = ResolveMenuTourStep(bInOffice, Menu != nullptr,
			HasDerivedLatch(TutorialDerivedLatch::MenuIntroDone));
		switch (Step.Index)
		{
		case MenuTourGuide::ExitOffice: ExitOfficeAt(MenuTourGuide::ExitOffice); break;
		case MenuTourGuide::OpenMenu:   Step.Anchor = Dock ? Dock->GetMenuButtonWidget() : CloseFirst(FindTutorialPanelCloseButton(TopBottom)); break;
		case MenuTourGuide::CloseMenu:  Step.Anchor = Menu->GetCloseButtonWidget(); break;
		default: break; // 설명 걸음 — 코치마크가 진행한다
		}
		return true;
	}

	// 아래 사무실 조건은 성장 도움말 걸음을 그대로 쓴다 — 회사 지정·도시 쪽 정리·성장 도움말이 못 보는 화면만 여기서 덧댄다
	case EMissionConditionType::RecruitGoalDiscipline:
	case EMissionConditionType::LevelUpFirstEmployee:
	case EMissionConditionType::AllocateEmployeeStatPoint:
	case EMissionConditionType::EnhanceFirstEmployee:
	case EMissionConditionType::UpgradeFirstWorkstation:
	case EMissionConditionType::LaunchGoalProject:
	{
		int32 Hired = 0;
		// 방금 뽑은 직원으로 정원이 차도 리빌이 끝나면 미션이 넘어간다 — 그 사이 「정원이 찼어요」로 도시를 보내지 않는다
		if (Spec.Condition == EMissionConditionType::RecruitGoalDiscipline && Spec.BuildingIndex != INDEX_NONE)
		{
			const UEmployeeManager* HiredEmployees = GI->GetSubsystem<UEmployeeManager>();
			if (HiredEmployees)
			{
				for (const FEmployeeInstance& Employee : HiredEmployees->GetEmployeesInBuilding(Spec.BuildingIndex))
				{
					if (GetPrimaryDisciplineSlot(Employee) == static_cast<int32>(Spec.Discipline))
					{
						++Hired;
					}
				}
			}
			if (Hired >= Target)
			{
				Step.Index = INDEX_NONE;
				return true;
			}
		}
		if (!bInOffice && OtherPanel)
		{
			Step.Index = Spec.Condition == EMissionConditionType::RecruitGoalDiscipline ? RecruitDisciplineGuide::EnterOffice
				: Spec.Condition == EMissionConditionType::UpgradeFirstWorkstation ? DeskUpgradeGuide::EnterOffice
				: Spec.Condition == EMissionConditionType::LaunchGoalProject ? LaunchGoalGuide::EnterOffice
				: EmployeeGrowthGuide::EnterOffice;
			Step.Anchor = CloseFirst(OtherPanel->GetCloseButtonWidget());
			return true;
		}
		const UEmployeeManager* Staff = GI->GetSubsystem<UEmployeeManager>();
		const int32 StaffBuilding = Spec.BuildingIndex != INDEX_NONE ? Spec.BuildingIndex : GI->GetCurrentManagedBuildingIndex();
		const bool bSeatOpen = Staff && StaffBuilding >= 0
			&& Staff->GetEmployeeCountInBuilding(StaffBuilding) < Staff->GetBuildingEmployeeCapacity(StaffBuilding, false);
		if (Spec.Condition == EMissionConditionType::RecruitGoalDiscipline && bInTargetOffice && bSeatOpen
			&& !HasVacantWorkstation(CurWorld))
		{
			// 성장 도움말은 [책상]까지만 짚는다 — 필수 튜토리얼은 카탈로그·배치 확정까지 이어 줘야 멈추지 않는다
			if (GetTutorialPlacementMode(GI) == EPlacementMode::Workstation)
			{
				Step.Index = RecruitDisciplineGuide::ConfirmVacantDesk;
				Step.Anchor = BuildPlacementPanelWeak.IsValid() ? BuildPlacementPanelWeak->GetPlaceButtonWidget() : nullptr;
				return true;
			}
			if (UOfficeWorkstationPanelWidget* DeskCatalog = Cast<UOfficeWorkstationPanelWidget>(TopBottom))
			{
				Step.Index = RecruitDisciplineGuide::PickVacantDesk;
				Step.Anchor = DeskCatalog->GetDeskGuideCatalogWidget();
				return true;
			}
		}
		// 한 명만 더 필요하면 일괄 채용으로 정원을 넘겨 채우지 않는다 ― 다음 미션이 다시 「정원이 찼어요」로 돌아간다
		if (Spec.Condition == EMissionConditionType::RecruitGoalDiscipline && bInTargetOffice && Spec.BuildingIndex != INDEX_NONE
			&& Target - Hired == 1 && GoalRecruitClickedDiscipline == Spec.Discipline)
		{
			UOfficeRecruitmentModalWidget* Modal = RecruitModalWeak.Get();
			if (Modal && Modal->IsActivated())
			{
				Step.Index = RecruitDisciplineGuide::PressPull;
				Step.Anchor = Modal->GetPullButtonSingleWidget();
				return true;
			}
		}
		if (Spec.Condition == EMissionConditionType::EnhanceFirstEmployee && bInTargetOffice)
		{
			// 사무실 직원 패널의 별 강화 카드는 활성화하지 않는 오버레이라 성장 도움말 판정이 못 본다
			UEnhanceStarforceModalWidget* StarCard = StarforceModalWeak.Get();
			if (StarCard && (StarCard->IsActivated() || StarCard->GetParent() != nullptr))
			{
				Step.Index = EmployeeGrowthGuide::PressStarAttempt;
				Step.Anchor = StarCard->GetEnhanceActionButtonWidget();
				return true;
			}
		}
		if (Spec.Condition == EMissionConditionType::LaunchGoalProject && bInTargetOffice)
		{
			const UOfficeStageProgressManager* Stage = CurWorld ? CurWorld->GetSubsystem<UOfficeStageProgressManager>() : nullptr;
			const EProjectLifecycle Lifecycle = Stage ? Stage->GetLifecycle() : EProjectLifecycle::Idle;
			if (Lifecycle == EProjectLifecycle::LaunchPending && Stage->IsLaunchBlocked())
			{
				ULaunchConfirmWidget* FailedLaunch = LaunchConfirmWeak.Get();
				Step.Index = LaunchGoalGuide::DiscardFailed;
				Step.Anchor = FailedLaunch && FailedLaunch->IsActivated() ? FailedLaunch->GetDiscardButtonWidget() : nullptr;
				return true;
			}
			if (Lifecycle == EProjectLifecycle::Developing)
			{
				// 목표 프로젝트 개발 중에 「첫 게임 운영이 끝나면」 기다림 문구는 틀린 말이다 — [출시] 대기로 읽는다
				Step.Index = ResolveForcedBusyProjectStep(Stage->GetProgressData().ProjectID == Spec.TargetProjectIndex);
				Step.Anchor = OfficeDock ? OfficeDock->GetStripRequirementRowWidget() : nullptr;
				Step.bObserve = Step.Anchor != nullptr;
				return true;
			}
			// 앞 게임 운영이 끝나야 새 기획안을 연다 ― 말없이 기다리게 두지 않고 남은 시간 옆에서 이유를 말한다
			if (Lifecycle == EProjectLifecycle::Operating && Stage->GetProgressData().ProjectID != Spec.TargetProjectIndex)
			{
				Step.Index = LaunchGoalGuide::WaitRunning;
				Step.Anchor = OfficeDock ? OfficeDock->GetStripOperationTimeWidget() : nullptr;
				Step.bObserve = Step.Anchor != nullptr;
				return true;
			}
			// 카드가 미달 경고로 「그래도 개발」을 띄우면 그 말 그대로 안내한다 ― 예상치는 편차를 뺀 보수적 값이다
			UPitchBoardWidget* RiskBoard = PitchBoardWeak.Get();
			if (Lifecycle == EProjectLifecycle::Idle && RiskBoard && RiskBoard->IsActivated()
				&& RiskBoard->IsProjectCardGateRisk(Spec.TargetProjectIndex))
			{
				Step.Index = LaunchGoalGuide::PressStartRisk;
				Step.Anchor = RiskBoard->GetProjectCardCtaWidget(Spec.TargetProjectIndex);
				return true;
			}
		}
		return false;
	}

	default:
		return false;
	}
}

bool UMissionManagerSubsystem::ResolveGoalProjectPreparation(const FContentGuideSpec& Spec, FGoalGuideStep& OutStep) const
{
	if (Spec.bForcedTutorial) { return false; }
	const UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance());
	UWorld* CurrentWorld = GetWorld();
	const UOfficeStageProgressManager* Stage = CurrentWorld
		? CurrentWorld->GetSubsystem<UOfficeStageProgressManager>() : nullptr;
	if (!GI || !GI->IsInOfficeMap() || !Stage || Stage->GetLifecycle() != EProjectLifecycle::Idle) { return false; }
	const int32 BuildingIndex = GI->GetCurrentManagedBuildingIndex();
	const double Now = FPlatformTime::Seconds();
	if (GoalProjectEstimateWorld.Get() != CurrentWorld || GoalProjectEstimateBuilding != BuildingIndex
		|| GoalProjectEstimateIndex != Spec.TargetProjectIndex || Now - GoalProjectEstimateAt >= 1.0)
	{
		GoalProjectEstimateWorld = CurrentWorld;
		GoalProjectEstimateBuilding = BuildingIndex;
		GoalProjectEstimateIndex = Spec.TargetProjectIndex;
		GoalProjectEstimateAt = Now;
		bGoalProjectNeedsPreparation = false;
		GoalProjectWeakDiscipline = INDEX_NONE;
		const UTableManagerSubsystem* Tables = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
		bool bFound = false;
		const FProjectData Project = Tables ? Tables->GetProjectData(Spec.Industry, Spec.TargetProjectIndex, bFound) : FProjectData();
		if (bFound)
		{
			TArray<FEstimateWorkerInput> Roster;
			Stage->BuildContributorRoster(Roster);
			const FProjectOutlookEstimate Outlook = Stage->EstimateProjectOutlook(Roster, Project);
			bGoalProjectNeedsPreparation = !Outlook.bIsValid || Outlook.bGateRisk;
			const TArray<int32> Weights{Project.Weight_Plan, Project.Weight_Dev, Project.Weight_Graphics,
				Project.Weight_Sound, Project.Weight_Server, Project.Weight_QA};
			const int32 MaxWeight = FMath::Max(Weights);
			const float TargetBase = FStageProgressData::ComputeDisciplineTargetBase(
				Project.GetRequiredScore(1), Project.GetRequiredScore(2), Project.GetRequiredScore(3));
			TArray<float> Targets;
			for (int32 Weight : Weights) { Targets.Add(FStageProgressData::ComputeDisciplineTarget(Weight, MaxWeight, TargetBase)); }
			float WorstRatio = 0.f;
			PitchBoardText::FindWorstDiscipline(Outlook.ExpectedDisciplineScores, Targets, GoalProjectWeakDiscipline, WorstRatio);
		}
	}
	UWorkstationInfoWidget* DeskPanel = WorkstationPanelWeak.Get();
	if (DeskPanel && !DeskPanel->IsActivated()) { DeskPanel = nullptr; }
	if (!bGoalProjectNeedsPreparation)
	{
		if (!DeskPanel) { return false; }
		OutStep.Index = LaunchGoalGuide::Goal::ClosePreparationPanel;
		OutStep.Anchor = DeskPanel->GetCloseButtonWidget();
		return true;
	}
	if (UPitchBoardWidget* Board = PitchBoardWeak.Get(); Board && Board->IsActivated())
	{
		OutStep.Index = LaunchGoalGuide::Goal::CloseForPreparation;
		OutStep.Anchor = Board->GetCloseButtonWidget();
		return true;
	}
	if (UOfficeEmployeePanelWidget* EmployeePanel = OfficeEmployeePanelWeak.Get(); EmployeePanel && EmployeePanel->IsActivated())
	{
		OutStep.Index = LaunchGoalGuide::Goal::PreparationAlternatives;
		OutStep.Anchor = EmployeePanel;
		OutStep.bObserve = true;
		return true;
	}
	UEmployeeManager* Employees = GetGameInstance()->GetSubsystem<UEmployeeManager>();
	UOfficeManager* Office = CurrentWorld->GetSubsystem<UOfficeManager>();
	AWorkstationActorBase* PreparationDesk = nullptr;
	int32 BestPoints = 0;
	if (Employees && Office && GoalProjectWeakDiscipline != INDEX_NONE)
	{
		for (AWorkstationActorBase* Desk : Office->GetPlacedWorkstations())
		{
			if (!CGRGoalGuideStep::CanGuideDeskGrowth(Desk, false)
				&& !CGRGoalGuideStep::CanGuideDeskGrowth(Desk, true)) { continue; }
			for (int32 SeatIndex = 0; SeatIndex < Desk->GetChairCount(); ++SeatIndex)
			{
				const FEmployeeInstance* Employee = Employees->FindEmployee(Desk->GetAssignedEmployeeID(SeatIndex));
				const int32 Points = Employee && Employee->DisciplinePoints.IsValidIndex(GoalProjectWeakDiscipline)
					? Employee->DisciplinePoints[GoalProjectWeakDiscipline] : 0;
				if (Points > BestPoints) { BestPoints = Points; PreparationDesk = Desk; }
			}
		}
	}
	if (DeskPanel)
	{
		const bool bTargetDesk = PreparationDesk && DeskPanel->GetWorkstation() == PreparationDesk;
		OutStep.Index = bTargetDesk ? LaunchGoalGuide::Goal::UpgradePreparationDesk : LaunchGoalGuide::Goal::CloseForPreparation;
		OutStep.Anchor = bTargetDesk ? DeskPanel->GetFirstAffordableGrowthButton(false) : DeskPanel->GetCloseButtonWidget();
		if (bTargetDesk && !OutStep.Anchor)
		{
			OutStep.Index = LaunchGoalGuide::Goal::ReducePreparationBulk;
			OutStep.Anchor = DeskPanel;
			OutStep.bObserve = true;
		}
	}
	else if (PreparationDesk)
	{
		OutStep.Index = LaunchGoalGuide::Goal::PickPreparationDesk;
		OutStep.Spot = PreparationDesk;
	}
	else
	{
		OutStep.Index = LaunchGoalGuide::Goal::PreparationAlternatives;
		OutStep.Anchor = OfficeMainWeak.IsValid() ? OfficeMainWeak->GetEmployeeButtonWidget() : nullptr;
	}
	return true;
}

FGoalGuideStep UMissionManagerSubsystem::ResolveContentStep(const FContentGuideSpec& Spec) const
{
	FGoalGuideStep Step;

	UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance());
	if (!GI)
	{
		return Step;
	}
	// 필수 튜토리얼만 성장 도움말이 플레이어 선택에 맡기던 대상까지 짚는다. 성장 도움말(false)은 아래 공용 판정 그대로
	if (Spec.bForcedTutorial && ResolveForcedTutorialStep(Spec, Step))
	{
		return Step;
	}

	// 판정에 쓰는 것은 전부 "지금 무엇이 열려 있는가" — 저장된 진행도가 아니다.
	// 그래서 플레이어가 어디로 새든 다음 조회에서 맞는 걸음이 나온다(후퇴 로직 불필요)
	const bool bInOffice = GI->IsInOfficeMap();
	const int32 GuideBuildingIndex = Spec.BuildingIndex;
	const bool bInTargetOffice = bInOffice && (GuideBuildingIndex == INDEX_NONE
		|| GI->GetCurrentManagedBuildingIndex() == GuideBuildingIndex);
	UBuildingManagePanelWidget* Manage = ManagePanelWeak.Get();
	if (Manage && !Manage->IsActivated()) Manage = nullptr;
	UBuildOpenWidget* Dock = BuildOpenWidgetWeak.Get();
	UOfficeMainWidget* OfficeDock = OfficeMainWeak.Get();

	// 0번은 항상 장소 이동 — 도시 목표인데 사무실에 있으면 [나가기] 로 되돌린다
	auto ExitOffice = [this, &Step]()
	{
		Step.Index = 0;
		Step.Anchor = OfficeLayerWeak.IsValid() ? OfficeLayerWeak->GetBackButtonWidget() : nullptr;
	};
	// 사무실 목표인데 도시에 있으면: 관리 패널이 열려 있으면 [입장], 아니면 회사 자체를 비춘다
	auto EnterOffice = [this, Manage, GuideBuildingIndex, &Step]()
	{
		Step.Index = 0;
		if (Manage && (GuideBuildingIndex == INDEX_NONE ||
			(Manage->GetTargetBuilding() && Manage->GetTargetBuilding()->GetBuildingIndex() == GuideBuildingIndex)))
		{
			Step.Anchor = Manage->GetEnterOfficeButtonWidget();
		}
		else
		{
			UEntityManager* Entities = GetWorld() ? GetWorld()->GetSubsystem<UEntityManager>() : nullptr;
			Step.Spot = GuideBuildingIndex != INDEX_NONE && Entities
				? Entities->GetBuildingByIndex(GuideBuildingIndex) : PickGuideBuilding();
		}
	};

	switch (Spec.Condition)
	{
	case EMissionConditionType::PlaceFirstPainting:        // G7 그림 — 사무실
		if (!bInOffice) { EnterOffice(); }
		else { Step.Index = 1; Step.Anchor = OfficeDock ? OfficeDock->GetDecorationOpenButtonWidget() : nullptr; }
		break;

	case EMissionConditionType::UpgradeFactoryStats:
	case EMissionConditionType::ReachFactoryProductionLevel:
		if (bInOffice) { ExitOffice(); }
		else if (UFactoryPanelWidget* FactoryPanel = FactoryPanelWeak.Get(); FactoryPanel && FactoryPanel->IsActivated())
		{
			Step.Index = 2;
			Step.Anchor = Spec.Condition == EMissionConditionType::ReachFactoryProductionLevel
				? FactoryPanel->GetUpgradeTarget(EFactoryUpgradeType::HoldProductionAmount)
				: FactoryPanel->GetFirstAffordableUpgradeTarget();
			if (!Step.Anchor) { Step.Anchor = FactoryPanel; Step.bObserve = true; }
		}
		else
		{
			Step.Index = 1;
			Step.Anchor = Dock ? Dock->GetFactoryOpenButtonWidget() : nullptr;
		}
		break;

	case EMissionConditionType::CollectMineResource:
	case EMissionConditionType::CollectFactoryProduct:
	case EMissionConditionType::SellManufacturedProduct:
	{
		if (bInOffice) { ExitOffice(); break; }
		UUIManagerSubsystem* UIManager = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
		UUIBase* UIBase = UIManager ? UIManager->GetUIBase() : nullptr;
		UCommonActivatableWidget* Prompt = UIBase ? UIBase->GetActivePromptWidget() : nullptr;
		UWorldMapPanelWidget* Map = Cast<UWorldMapPanelWidget>(Prompt);
		UCountryDetailWidget* Detail = Cast<UCountryDetailWidget>(Prompt);
		if (!Detail && Map) { Detail = Cast<UCountryDetailWidget>(Map->GetActiveDrawerWidget()); }
		const UTableManagerSubsystem* Tables = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
		const UMineManager* Mines = GetGameInstance()->GetSubsystem<UMineManager>();
		const UWorldFactoryManager* Factories = GetGameInstance()->GetSubsystem<UWorldFactoryManager>();
		const UProductionOrderManager* Orders = GetGameInstance()->GetSubsystem<UProductionOrderManager>();
		const TArray<FCountryInfoTable> Countries = Tables ? Tables->GetAllCountryInfos() : TArray<FCountryInfoTable>();
		TArray<TPair<EResourceType, int32>> RecipeMaterials;
		TArray<EResourceType> NeededMaterials;
		int32 TargetProductIndex = Spec.TargetProjectIndex;
		int32 TargetQuantity = 1;
		bool bHasPendingOrder = false;
		bool bHasRecipe = false;
		bool bHasMatchingFactoryLine = false;
		const bool bMineGoal = Spec.Condition == EMissionConditionType::CollectMineResource;
		const bool bFactoryGoal = Spec.Condition == EMissionConditionType::CollectFactoryProduct;
		if ((bMineGoal || bFactoryGoal) && Spec.Industry != ECompanyType::None)
		{
			if (Orders)
			{
				for (const FProductionOrder& Order : Orders->GetAllOrders())
				{
					if (Order.CompanyType != Spec.Industry || Order.RemainingQuantity <= 0
						|| (TargetProductIndex > 0 && Order.ProjectIndex != TargetProductIndex)
						|| (Spec.BuildingIndex != INDEX_NONE && Order.SourceBuildingID != Spec.BuildingIndex)) { continue; }
					TargetProductIndex = Order.ProjectIndex;
					TargetQuantity = bMineGoal ? Order.RemainingQuantity : 1;
					bHasPendingOrder = true;
					break;
				}
			}
			if (Factories)
			{
				for (const FCountryInfoTable& CountryRow : Countries)
				{
					for (const FWorldFactoryLineState& Line : Factories->GetActiveLines(CountryRow.CountryType))
					{
						if (Line.CompanyType != Spec.Industry || (TargetProductIndex > 0 && Line.ProductIndex != TargetProductIndex)) { continue; }
						bHasMatchingFactoryLine = true;
						if (TargetProductIndex <= 0) { TargetProductIndex = Line.ProductIndex; }
						break;
					}
					if (bHasMatchingFactoryLine) { break; }
				}
			}
			// 제품 미지정 반복 목표에는 해당 업종의 첫 레시피를 사용한다.
			if (TargetProductIndex <= 0) { TargetProductIndex = 1; }
			const FProductRecipeTable Recipe = Tables
				? Tables->GetProductRecipe(Spec.Industry, TargetProductIndex, bHasRecipe) : FProductRecipeTable();
			if (bHasRecipe)
			{
				Recipe.CollectMaterials(RecipeMaterials);
				const UResourceItemManager* Resources = GetGameInstance()->GetSubsystem<UResourceItemManager>();
				const TMap<EResourceType, int64> Inventory = Resources ? Resources->GetAllResources() : TMap<EResourceType, int64>();
				NeededMaterials = ManufacturingGuideCountryRules::MissingMaterials(RecipeMaterials, Inventory, TargetQuantity);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[GoalGuide] Missing recipe for industry %d, product %d"),
					static_cast<int32>(Spec.Industry), TargetProductIndex);
			}
		}
		const bool bRecoverMaterials = bFactoryGoal && ManufacturingGuideCountryRules::ShouldRecoverMaterials(
			bHasMatchingFactoryLine, bHasPendingOrder, bHasRecipe, NeededMaterials);
		if (Detail)
		{
			Step.Index = 3;
			if (Detail->IsProductionStartPopupOpen()
				|| (bMineGoal && Detail->GetCurrentTabIndex() != UCountryDetailWidget::TAB_INDEX_MINE))
			{
				Step.bObserve = true;
				break;
			}
			const FCountryInfoTable* DetailCountry = Countries.FindByPredicate([Detail](const FCountryInfoTable& Row)
			{
				return Row.CountryType == Detail->GetCurrentCountry();
			});
			const bool bCanMineNeeded = DetailCountry && DetailCountry->bSupportsMine
				&& NeededMaterials.ContainsByPredicate([DetailCountry](EResourceType Resource)
				{
					return DetailCountry->MinableResources.Contains(Resource);
				});
			const bool bWrongFactory = bFactoryGoal && !bRecoverMaterials && DetailCountry
				&& !ManufacturingGuideCountryRules::SupportsFactoryIndustry(*DetailCountry, Spec.Industry);
			if (bFactoryGoal && ((bRecoverMaterials && !bCanMineNeeded)
				|| (!bRecoverMaterials && (bWrongFactory || Detail->GetCurrentTabIndex() == UCountryDetailWidget::TAB_INDEX_MINE))))
			{
				Step.Index = 5;
				UPanelHeaderWidget* Header = Cast<UPanelHeaderWidget>(Detail->GetWidgetFromName(TEXT("PanelHeader")));
				Step.Anchor = Header ? VisibleOrNull(Header->GetCloseButton()) : nullptr;
				if (!Step.Anchor) { Step.Anchor = Detail; Step.bObserve = true; }
				break;
			}
			if (bRecoverMaterials) { Step.Index = 4; }
			const int32 WantedTab = (bMineGoal || bRecoverMaterials)
				? UCountryDetailWidget::TAB_INDEX_MINE
				: bFactoryGoal
					? UCountryDetailWidget::TAB_INDEX_FACTORY : UCountryDetailWidget::TAB_INDEX_PORT;
			const FName TargetName = Detail->GetCurrentTabIndex() == WantedTab
				? bMineGoal || bRecoverMaterials ? TEXT("MineHero")
					: bFactoryGoal ? TEXT("FactoryHero") : TEXT("PortHero")
				: bMineGoal || bRecoverMaterials ? TEXT("MineTab")
					: bFactoryGoal ? TEXT("FactoryTab") : TEXT("PortTab");
			UWidget* Target = VisibleOrNull(Detail->GetWidgetFromName(TargetName));
			const FVector2D TargetSize = Target ? Target->GetCachedGeometry().GetLocalSize() : FVector2D::ZeroVector;
			Step.Anchor = TargetSize.X > 1.f && TargetSize.Y > 1.f ? Target : nullptr;
			if (!Step.Anchor) { Step.Anchor = Detail; Step.bObserve = true; }
			else if (Detail->GetCurrentTabIndex() == WantedTab) { Step.bObserve = true; }
		}
		else if (Map)
		{
			if (Map->IsDrawerOpen())
			{
				Step.Index = 3;
				Step.Anchor = Map;
				Step.bObserve = true;
				break;
			}
			Step.Index = 2;
			UWidget* FirstEligible = nullptr;
			UWidget* BestMedallion = nullptr;
			int32 BestPriority = INDEX_NONE;
			for (const FCountryInfoTable& CountryRow : Countries)
			{
				const bool bSelectMine = bMineGoal || bRecoverMaterials;
				const bool bSupportsTarget = bSelectMine
					? CountryRow.bSupportsMine
					: bFactoryGoal
						? ManufacturingGuideCountryRules::SupportsFactoryIndustry(CountryRow, Spec.Industry) : CountryRow.bSupportsTrade;
				if (!bSupportsTarget || !OverseasActions::IsCountryUnlocked(GetGameInstance(), CountryRow.CountryType)) { continue; }
				const TObjectPtr<UCountryNameWidget>* Medallion = Map->GetMedallions().Find(CountryRow.CountryType);
				if (!Medallion || !Medallion->Get() || !Medallion->Get()->IsVisible()) { continue; }
				const FVector2D PinSize = Medallion->Get()->GetCachedGeometry().GetLocalSize();
				if (PinSize.X <= 1.f || PinSize.Y <= 1.f) { continue; }
				if (!FirstEligible && !bSelectMine) { FirstEligible = Medallion->Get(); }
				int32 Priority = 0;
				if (bSelectMine)
				{
					TArray<EResourceType> ActiveResources;
					if (Mines)
					{
						for (const FMineLineState& Line : Mines->GetMineLines(CountryRow.CountryType))
						{
							ActiveResources.Add(Line.Resource);
						}
					}
					Priority = ManufacturingGuideCountryRules::MinePriority(CountryRow, RecipeMaterials, NeededMaterials, ActiveResources);
					if (Priority == INDEX_NONE) { continue; }
				}
				else if (bFactoryGoal)
				{
					for (const FWorldFactoryLineState& Line : Factories ? Factories->GetActiveLines(CountryRow.CountryType) : TArray<FWorldFactoryLineState>())
					{
						if ((Spec.Industry == ECompanyType::None || Line.CompanyType == Spec.Industry)
							&& (TargetProductIndex <= 0 || Line.ProductIndex == TargetProductIndex)) { Priority = 1; break; }
					}
				}
				else { Priority = CountryRow.bIsTradeHub ? 1 : 0; }
				if (Priority > BestPriority) { BestPriority = Priority; BestMedallion = Medallion->Get(); }
			}
			Step.Anchor = BestMedallion ? BestMedallion : FirstEligible ? FirstEligible : static_cast<UWidget*>(Map);
			Step.bObserve = Step.Anchor == Map;
		}
		else
		{
			Step.Index = 1;
			Step.Anchor = Dock ? Dock->GetWorldMapButtonWidget() : nullptr;
		}
		break;
	}

	case EMissionConditionType::ReserveManufacturingProduct:
	{
		if (!bInTargetOffice) { bInOffice ? ExitOffice() : EnterOffice(); break; }
		UPitchBoardWidget* Board = PitchBoardWeak.Get();
		if (!Board || !Board->IsActivated())
		{
			Step.Index = 1;
			Step.Anchor = OfficeDock ? OfficeDock->GetOpenBoardButtonWidget() : nullptr;
			break;
		}
		const int32 ProductIndex = FMath::Max(1, Spec.TargetProjectIndex);
		Step.Anchor = Board->GetProjectCardCtaWidget(ProductIndex);
		if (!Step.Anchor)
		{
			Step.Index = 2;
			Step.Anchor = Board->GetProjectTileWidget(ProductIndex);
			if (!Step.Anchor) { Step.Anchor = Board; Step.bObserve = true; }
			break;
		}
		const UManufacturingDevelopmentManager* Development = GetGameInstance()->GetSubsystem<UManufacturingDevelopmentManager>();
		const FManufacturingResearchStatus Research = Development
			? Development->GetResearchStatus(GuideBuildingIndex != INDEX_NONE
				? GuideBuildingIndex : GI->GetCurrentManagedBuildingIndex(), ProductIndex)
			: FManufacturingResearchStatus();
		Step.Index = Research.bResearched ? 4 : 3;
		if (Research.bInProgress) { Step.Anchor = Board; Step.bObserve = true; }
		break;
	}

	case EMissionConditionType::LevelUpFirstEmployee:
	case EMissionConditionType::AllocateEmployeeStatPoint:
	case EMissionConditionType::EnhanceFirstEmployee:
	{
		const bool bStarGoal = Spec.Condition == EMissionConditionType::EnhanceFirstEmployee;
		UEmployeeWindowWidget* EmpWin = EmployeeWindowWeak.Get();
		if (EmpWin && !EmpWin->IsActivated()) EmpWin = nullptr;
		UEnhanceStarforceModalWidget* Starforce = StarforceModalWeak.Get();
		if (Starforce && !Starforce->IsActivated()) Starforce = nullptr;
		// 직원창이 없고 사무실 직원 패널이 떠 있으면 패널 버튼을 앵커로 쓴다
		UOfficeEmployeePanelWidget* EmpPanel = EmpWin ? nullptr : OfficeEmployeePanelWeak.Get();
		if (EmpPanel && EmpPanel->IsActivated())
		{
			Step.Index = CGRGoalGuideStep::ResolveEmployeeGrowthStep(bInTargetOffice, true, Starforce != nullptr, bStarGoal);
			if (Step.Index == 0) { bInOffice ? ExitOffice() : EnterOffice(); }
			else if (Step.Index == 3) { Step.Anchor = Starforce->GetEnhanceActionButtonWidget(); }
			else if (bStarGoal) { Step.Anchor = EmpPanel->GetEnhanceButtonWidget(); }
			else
			{
				UWidget* GrowthButton = Spec.Condition == EMissionConditionType::AllocateEmployeeStatPoint
					? EmpPanel->GetStatPointTargetWidget() : EmpPanel->GetLevelUpButtonWidget();
				if (GrowthButton && GrowthButton->GetIsEnabled()) Step.Anchor = GrowthButton;
				else Step.Index = 3;
			}
			break;
		}
		Step.Index = CGRGoalGuideStep::ResolveEmployeeGrowthStep(
			bInTargetOffice, EmpWin != nullptr, Starforce != nullptr, bStarGoal);
		if (Step.Index == 0) { bInOffice ? ExitOffice() : EnterOffice(); }
		else if (Step.Index == 1) { Step.Anchor = OfficeDock ? OfficeDock->GetEmployeeButtonWidget() : nullptr; }
		else if (Step.Index == 3) { Step.Anchor = Starforce->GetEnhanceActionButtonWidget(); }
		else if (bStarGoal) { Step.Anchor = EmpWin->GetEnhanceButtonWidget(); }
		else
		{
			UWidget* LevelUpButton = Spec.Condition == EMissionConditionType::AllocateEmployeeStatPoint
				? EmpWin->GetStatPointTargetWidget() : EmpWin->GetLevelUpButtonWidget();
			if (LevelUpButton && LevelUpButton->GetIsEnabled()) Step.Anchor = LevelUpButton;
			else Step.Index = 3; // 선택 직원을 바꾸는 안내 문구. 별 목표의 3번과 데이터 행이 독립이다.
		}
		break;
	}

	case EMissionConditionType::UpgradeFirstWorkstation:
	case EMissionConditionType::PromoteFirstWorkstation:
	{
		if (!bInTargetOffice) { bInOffice ? ExitOffice() : EnterOffice(); break; }
		const bool bPromotion = Spec.Condition == EMissionConditionType::PromoteFirstWorkstation;
		AWorkstationActorBase* GuideDesk = nullptr;
		if (UOfficeManager* Office = GetWorld() ? GetWorld()->GetSubsystem<UOfficeManager>() : nullptr)
		{
			for (AWorkstationActorBase* Desk : Office->GetPlacedWorkstations())
			{
				if (CGRGoalGuideStep::CanGuideDeskGrowth(Desk, bPromotion)) { GuideDesk = Desk; break; }
			}
		}
		UWorkstationInfoWidget* Panel = WorkstationPanelWeak.Get();
		if (Panel && Panel->IsActivated())
		{
			if (GuideDesk && !CGRGoalGuideStep::CanGuideDeskGrowth(Panel->GetWorkstation(), bPromotion))
			{
				Step.Index = 3;
				Step.Anchor = Panel->GetCloseButtonWidget();
			}
			else
			{
				Step.Index = 2;
				Step.Anchor = Panel->GetFirstAffordableGrowthButton(bPromotion);
			}
		}
		else
		{
			Step.Index = 1;
			Step.Spot = GuideDesk;
			if (!Step.Spot) Step.Anchor = OfficeDock ? OfficeDock->GetWorkstationOpenButtonWidget() : nullptr;
		}
		break;
	}

	case EMissionConditionType::RecruitGoalDiscipline:
	{
		UEmployeeManager* Employees = GetGameInstance()->GetSubsystem<UEmployeeManager>();
		const int32 BuildingIndex = GuideBuildingIndex != INDEX_NONE
			? GuideBuildingIndex : GI->GetCurrentManagedBuildingIndex();
		const int32 Capacity = Employees && BuildingIndex >= 0
			? Employees->GetBuildingEmployeeCapacity(BuildingIndex, false) : 0;
		if (Capacity > 0 && Employees->GetEmployeeCountInBuilding(BuildingIndex) >= Capacity)
		{
			Step.Index = 1;
			if (bInOffice)
			{
				Step.Anchor = OfficeLayerWeak.IsValid() ? OfficeLayerWeak->GetBackButtonWidget() : nullptr;
			}
			else if (Manage && (GuideBuildingIndex == INDEX_NONE
				|| (Manage->GetTargetBuilding() && Manage->GetTargetBuilding()->GetBuildingIndex() == GuideBuildingIndex)))
			{
				Step.Index = Manage->IsEnhancementTabActive() ? 3 : 2;
				Step.Anchor = Step.Index == 3 ? Manage->GetBuildingFloorUpgradeButtonWidget()
					: Manage->GetEnhancementTabButtonWidget();
			}
			else
			{
				UEntityManager* Entities = GetWorld() ? GetWorld()->GetSubsystem<UEntityManager>() : nullptr;
				Step.Spot = GuideBuildingIndex != INDEX_NONE && Entities
					? Entities->GetBuildingByIndex(GuideBuildingIndex) : PickGuideBuilding();
			}
			break;
		}
		if (!bInTargetOffice) { bInOffice ? ExitOffice() : EnterOffice(); break; }
		if (!HasVacantWorkstation(GetWorld()))
		{
			Step.Index = 4;
			Step.Anchor = OfficeDock ? OfficeDock->GetWorkstationOpenButtonWidget() : nullptr;
		}
		else if (UOfficeRecruitmentModalWidget* Modal = RecruitModalWeak.Get(); Modal && Modal->IsActivated())
		{
			Step.Index = GoalRecruitClickedDiscipline == Spec.Discipline ? 7 : 6;
			if (Step.Index == 7)
			{
				int32 Hired = 0;
				if (Employees)
				{
					for (const FEmployeeInstance& Employee : Employees->GetEmployeesInBuilding(BuildingIndex))
					{
						if (GetPrimaryDisciplineSlot(Employee) == static_cast<int32>(Spec.Discipline)) { ++Hired; }
					}
				}
				Step.Anchor = FTutorialDerivedMissionRules::ResolveTarget(Spec.ConditionAmount) - Hired <= 1 ? Modal->GetPullButtonSingleWidget()
					: Modal->GetPullButtonMultiWidget();
			}
			else { Step.Anchor = Modal->GetJobSelectionWidget(Spec.Discipline); }
		}
		else
		{
			Step.Index = 5;
			Step.Anchor = OfficeDock ? OfficeDock->GetRecruitmentButtonWidget() : nullptr;
		}
		break;
	}

	case EMissionConditionType::LaunchGoalProject:
		if (!bInTargetOffice) { bInOffice ? ExitOffice() : EnterOffice(); }
		else if (ResolveGoalProjectPreparation(Spec, Step)) { break; }
		else if (const UOfficeStageProgressManager* Stage = GetWorld()
			? GetWorld()->GetSubsystem<UOfficeStageProgressManager>() : nullptr;
			Stage && Stage->GetLifecycle() != EProjectLifecycle::Idle)
		{
			if (Stage->GetLifecycle() == EProjectLifecycle::LaunchPending
				&& Stage->GetProgressData().ProjectID == Spec.TargetProjectIndex)
			{
				const bool bBlocked = Stage->IsLaunchBlocked();
				Step.Index = bBlocked ? LaunchGoalGuide::Goal::DiscardFailed : LaunchGoalGuide::PressLaunch;
				if (ULaunchConfirmWidget* Launch = LaunchConfirmWeak.Get(); Launch && Launch->IsActivated())
				{
					Step.Anchor = VisibleOrNull(bBlocked
						? Launch->GetDiscardButtonWidget() : Launch->GetConfirmButtonWidget());
				}
			}
			else { Step.Index = 5; }
		}
		else if (UPitchBoardWidget* Board = PitchBoardWeak.Get(); Board && Board->IsActivated())
		{
			Step.Anchor = Board->GetProjectCardCtaWidget(Spec.TargetProjectIndex);
			Step.Index = Step.Anchor ? 3 : 2;
			if (Step.Index == 2) Step.Anchor = Board->GetProjectTileWidget(Spec.TargetProjectIndex);
		}
		else { Step.Index = 1; Step.Anchor = OfficeDock ? OfficeDock->GetOpenBoardButtonWidget() : nullptr; }
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
	case EMissionConditionType::EnhanceBuildingStat:
	{
		const bool bFloorUpgrade = Spec.Condition == EMissionConditionType::RaiseBuildingFloor;
		UEntityManager* Entities = GetWorld() ? GetWorld()->GetSubsystem<UEntityManager>() : nullptr;
		ABuildingBaseActor* GuideBuilding = Entities ? Entities->GetBuildingByIndex(GuideBuildingIndex) : nullptr;
		if (!bInOffice && Manage && Manage->GetTargetBuilding() != GuideBuilding)
		{
			Step.Index = 4;
			Step.Anchor = Manage->GetCloseButtonWidget();
			break;
		}
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
			Step.Spot = GuideBuilding;
		}
		else if (Step.Index == 2)
		{
			Step.Anchor = Manage->GetEnhancementTabButtonWidget();
		}
		else
		{
			Step.Anchor = bFloorUpgrade ? Manage->GetBuildingFloorUpgradeButtonWidget()
				: Manage->GetFirstAffordableStatEnhanceButtonWidget();
		}
		break;
	}

	case EMissionConditionType::ReachHQLevel:
	{
		if (bInOffice) { ExitOffice(); }
		else
		{
			UUIManagerSubsystem* UIManager = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
			UUIBase* UIBase = UIManager ? UIManager->GetUIBase() : nullptr;
			UHQManagePanelWidget* HQ = UIBase ? Cast<UHQManagePanelWidget>(UIBase->GetActivePromptWidget()) : nullptr;
			bool bConditionsUnmet = false;
			Step.Anchor = HQ ? HQ->GetGrowthGuideTargetWidget(&bConditionsUnmet)
				: (Dock ? Dock->GetHeadquartersButtonWidget() : nullptr);
			Step.Index = HQ ? (HQ->IsGrowthTabActive() ? (bConditionsUnmet ? 4 : 3) : 2) : 1;
			Step.bObserve = bConditionsUnmet;
		}
		break;
	}

	case EMissionConditionType::BuildSecondBuilding:       // G3 / G6 건설 — 도크 [건설] → 모달(카드 지목 없음)
	case EMissionConditionType::BuildOnClearedPlot:
	case EMissionConditionType::BuildIndustry:
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

namespace CGRGoalGuideCopy
{
FText RecruitFullOfficeLine(const FText& DefaultLine, const FText& TargetCompanyName,
	bool bInOffice, int32 TargetBuildingIndex, int32 OpenBuildingIndex)
{
	return bInOffice && TargetBuildingIndex != INDEX_NONE && OpenBuildingIndex != TargetBuildingIndex
		&& !TargetCompanyName.IsEmpty()
		? FText::Format(NSLOCTEXT("GoalTracker", "FullDifferentCompany",
			"{0}의 정원이 찼어요. 도시에서 해당 회사를 키워요."), TargetCompanyName)
		: DefaultLine;
}
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
	const FText Line = Row.StepLines.IsValidIndex(Index) ? Row.StepLines[Index] : FText::GetEmpty();
	if (Row.ConditionType == EMissionConditionType::RecruitGoalDiscipline && Index == 1)
	{
		const UCGGameInstance* Game = Cast<UCGGameInstance>(GetGameInstance());
		const int32 TargetBuildingIndex = GoalMgr->GetGuideBuildingIndex(GoalMgr->GetTrackedGoalID());
		return CGRGoalGuideCopy::RecruitFullOfficeLine(Line,
			Game && TargetBuildingIndex != INDEX_NONE ? Game->GetCompanyDisplayName(TargetBuildingIndex) : FText::GetEmpty(),
			Game && Game->IsInOfficeMap(), TargetBuildingIndex,
			Game ? Game->GetCurrentManagedBuildingIndex() : INDEX_NONE);
	}
	return Line;
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

FText UMissionManagerSubsystem::GetCurrentGuideLine() const
{
	if (!HasActiveMission() || bReadyToClaim || bGuideSuspendedForLaunchReward
		|| bRecruitmentPresentationActive || !ActiveMissionRow.GuideLines.IsValidIndex(GuidePhase))
	{
		return FText::GetEmpty();
	}
	// 보상 띠가 화면을 가로지르는 동안엔 말풍선을 접는다 — 겹쳐 둘 다 안 읽힌다(띠가 걷히면 다시 등장)
	if (const UUIManagerSubsystem* ToastUI = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>(); ToastUI && ToastUI->IsRewardToastShown())
	{
		return FText::GetEmpty();
	}
	// 걸음 번호가 없는 순간 = 앞 화면 정리(창 닫기)나 기다림 — 걸음 문구를 남기면 대상과 다른 말을 한다
	if (IsDerivedGuideActive())
	{
		const FGoalGuideStep Targets = GetDerivedGuideTargets();
		if (Targets.Index == INDEX_NONE)
		{
			if (!Targets.Anchor)
			{
				return FText::GetEmpty();
			}
			// 결산 창은 지난 게임의 성적표다 ― 「창을 닫아요」만으로는 무엇을 본 건지 모른다
			const UUIManagerSubsystem* ReportUI = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
			const UUIBase* ReportBase = ReportUI ? ReportUI->GetUIBase() : nullptr;
			if (ReportBase && Cast<UProjectReportWidget>(ReportBase->GetActivePromptWidget()))
			{
				return NSLOCTEXT("MissionGuide", "DerivedCloseReport", "지난 게임의 <key>결산</>이에요. 확인하고 닫아요.");
			}
			// 별 강화 카드는 결과 연출(성공·유지·하락)을 보여 주고 그대로 열려 있다
			if (ActiveMissionRow.ConditionType == EMissionConditionType::EquipFirstEquipment
				&& StarforceModalWeak.IsValid() && StarforceModalWeak->GetParent())
			{
				return NSLOCTEXT("MissionGuide", "DerivedCloseStarResult", "별 강화를 마쳤어요. 이 창을 <key>닫아요</>.");
			}
			return NSLOCTEXT("MissionGuide", "DerivedCloseFirst", "먼저 이 창을 <key>닫아요</>.");
		}
	}
	// 재화가 모자라 누를 버튼이 없거나 꺼져 있으면 같은 문구 대신 모을 방법을 말한다
	if (IsDerivedGuideActive() && !ActiveMissionRow.ShortfallLine.IsEmpty()
		&& IsDerivedShortfallStep(GetDerivedGuideTargets()))
	{
		return ActiveMissionRow.ShortfallLine;
	}
	const FText& Line = ActiveMissionRow.GuideLines[GuidePhase];
	if (ActiveMissionRow.ConditionType == EMissionConditionType::CollectBricks && GuidePhase == BrickGuide::AddLaborer)
	{
		const ABrickFactory* Factory = Cast<ABrickFactory>(BrickFactoryWeak.Get());
		const int32 Capacity = Factory ? Factory->GetLaborerCapacity() : 0;
		return Capacity > 0 ? FText::Format(Line, FText::AsNumber(Capacity)) : FText::GetEmpty();
	}
	if (ActiveMissionRow.ConditionType == EMissionConditionType::ExplainPlot
		&& (GuidePhase == PlotExplainGuide::Explain || GuidePhase == PlotExplainGuide::CloseDialog))
	{
		const ACityPlotActor* Plot = FindTutorialClearedPlot(GetWorld());
		const UCityAcquisitionManager* PlotAcq = GetWorld() ? GetWorld()->GetSubsystem<UCityAcquisitionManager>() : nullptr;
		const int32 Remaining = Plot && PlotAcq ? PlotAcq->CountUnclearedPlotOccupants(Plot->GetPlotId()) : 0;
		return Remaining > 0 ? FText::Format(Line, FText::AsNumber(Remaining)) : FText::GetEmpty();
	}
	if (ActiveMissionRow.ConditionType == EMissionConditionType::PlaceDesks && GuidePhase == DeskGuide::PlaceMore)
	{
		const int32 Target = FMath::Max(1, static_cast<int32>(ActiveMissionRow.ConditionAmount));
		return FText::Format(Line, FText::AsNumber(FMath::Max(0, Target - DeskPlaceProgress)));
	}
	// 짚는 건물 카드 이름 ― 본사 성장으로 막 열린 건물을 이름으로 부른다
	if ((ActiveMissionRow.ConditionType == EMissionConditionType::BuildIndustry
			|| ActiveMissionRow.ConditionType == EMissionConditionType::BuildSecondBuilding)
		&& GuidePhase == SecondBuildingGuide::PickBuilding && Line.ToString().Contains(TEXT("{0}")))
	{
		// 도출형 미션은 BuildModalWeak 를 쓰지 않는다(첫 회사 전용) ― 걸음 도출과 같은 맨 위 창을 본다
		FText CardName;
		const UUIManagerSubsystem* CardUI = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
		const UUIBase* CardBase = CardUI ? CardUI->GetUIBase() : nullptr;
		if (const UBuildModalWidget* Modal = CardBase ? Cast<UBuildModalWidget>(CardBase->GetActivePromptWidget()) : nullptr)
		{
			Modal->GetBuildGuideBestCardWidget(&CardName);
		}
		return CardName.IsEmpty() ? FText::GetEmpty() : FText::Format(Line, CardName);
	}
	return Line;
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

	if (IsDerivedGuideActive() && !bReadyToClaim)
	{
		const FGoalGuideStep Targets = GetDerivedGuideTargets();
		if (Targets.Index == INDEX_NONE && Targets.Anchor)
		{
			return NSLOCTEXT("MissionGuide", "DerivedCloseFirstMentor", "열린 창을 닫고 이어서 진행하세요");
		}
	}
	if (IsDerivedGuideActive() && !bReadyToClaim && !ActiveMissionRow.ShortfallLine.IsEmpty()
		&& IsDerivedShortfallStep(GetDerivedGuideTargets()))
	{
		return ActiveMissionRow.ShortfallLine;
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
				const EProductionDiscipline Required = GetRequiredRecruitDiscipline();
				int32 Count = 0;
				for (const FEmployeeInstance& Employee : EmpMgr->GetAllEmployeesRef())
				{
					if (Employee.AssignedBuildingIndex == CGGI->GetCurrentManagedBuildingIndex()
						&& (Required == EProductionDiscipline::Count || DepartmentToDiscipline(Employee.Department) == Required)) ++Count;
				}
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

	const bool bWantGoalTracker = GoalMgr && !HasActiveMission() && GoalMgr->IsUnlocked() && GoalMgr->HasAvailableGuide();

	if (bWantGoalTracker && !GoalTrackerWidget)
	{
		if (TSubclassOf<UUserWidget> GoalTrackerClass = TableMgr->GetWidgetClass(EWidgetType::GoalTracker))
		{
			GoalTrackerWidget = CreateWidget<UGoalTrackerWidget>(PC, GoalTrackerClass);
			if (GoalTrackerWidget)
			{
				GoalTrackerWidget->InitTracker();
				// 마지막 미션 수령과 같은 순간에 생기므로 보상 창이 닫힐 때까지 숨긴다
				RefreshTrackerPresentationSuppression();

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
	if (bRecruitmentPresentationActive)
	{
		if (TrackerWidget) { TrackerWidget->SetPresentationSuppressed(true); }
		if (GoalTrackerWidget) { GoalTrackerWidget->SetPresentationSuppressed(true); }
		if (GuideOverlayWidget)
		{
			if (!bGuideOverlayVisibilitySavedForRecruitment)
			{
				GuideOverlayVisibilityBeforeRecruitment = GuideOverlayWidget->GetVisibility();
				bGuideOverlayVisibilitySavedForRecruitment = true;
			}
			GuideOverlayWidget->SetVisibility(ESlateVisibility::Hidden);
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
	bGuideOverlayVisibilitySavedForRecruitment = false;

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
		UIMgr->ClearGuideNotification();
		const FGoalGuideStep Step = GetTrackedGoalStep();
		if (IsValid(Step.Spot))
		{
			if (APlayerCamera* Camera = FindPlayerCamera(GetWorld()))
			{
				Camera->FocusOnActor(Step.Spot, 0.6f, false);
			}
		}
	}
	else
	{
		UIMgr->ClearGuideNotification();
	}
}
