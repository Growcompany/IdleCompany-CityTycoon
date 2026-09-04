// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Panel/BuildingManagePanelWidget.h"
#include "UI/Panel/BuildingEnhancementLockPolicy.h"
#include "Core/CGGameInstance.h"
#include "Player/MainMapPlayerController.h"
#include "Player/PlayerCamera.h"
#include "Player/Components/PlacementHandler.h"
#include "Entity/Building/BuildingBaseActor.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Buttons/TabButtonWidget.h"
#include "UI/Element/Building/BuildingSkinCardWidget.h"
#include "UI/Element/Building/BuildingLightCardWidget.h"
#include "UI/Element/Building/UpgradeSlot.h"
#include "UI/Element/Buttons/CostActionButtonWidget.h"
#include "UI/Panel/BuildPlacementWidget.h"
#include "UI/Panel/BuildPlacementPanelWidget.h"
#include "UI/Panel/TierRoadmapWidget.h"
#include "Manager/UIManagerSubsystem.h"
#include "Manager/EntityManager.h"
#include "Manager/KeystoneAuraSubsystem.h"
#include "Manager/MissionManagerSubsystem.h"
#include "UI/Panel/InGameLayerWidget.h"
#include "Manager/SoundManagerSubsystem.h"
#include "UI/UISoundTags.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/BuildingTraitManagerSubsystem.h"
#include "Table/BuildingTraitTable.h"
#include "UI/Element/Building/TraitCardSlotWidget.h"
#include "UI/Element/Building/TraitSlotWidget.h"
#include "UI/Element/Building/TraitDetailPopupWidget.h"
#include "UI/Element/Building/TraitSetChipWidget.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Enum/LootBoxRarity.h"
#include "UI/UIBase.h"
#include "Enum/WidgetType.h"
#include "Enum/ResourceType.h"
#include "Enum/NotificationType.h"
#include "Global/GlobalUtilFunctions.h"
#include "UI/Element/Common/ConfirmCancelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Components/WrapBox.h"
#include "Groups/CommonButtonGroupBase.h"
#include "Data/BuildingEnhancementData.h"
#include "Enum/CompanyType.h"
#include "Manager/EmployeeManager.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "Components/Button.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UI/Element/Common/StatRowWidget.h"
#include "CommonTextBlock.h"
#include "Manager/ProjectOperationManager.h"
#include "Manager/ProductionOrderManager.h"
#include "Components/Image.h"
#include "Engine/AssetManager.h"
#include "UI/Element/Buttons/IconWithButtonWidget.h"

void UBuildingManagePanelWidget::NativeOnActivated()
{
	// 자체 Show/Hide 애님(UIE_PanelBorder 오른쪽↔왼쪽)은 베이스가 재생. 여기선 입력 모드 복원 + 우측 컬럼 트윈을 추가.
	Super::NativeOnActivated();

	// 자식 위젯을 push 하면 컨테이너가 이 패널을 deactivate 해 Normal 로 내려가는데, 재활성 시 아무도 되돌리지 않는다
	UCGGameInstance* GameInstance = Cast<UCGGameInstance>(GetGameInstance());
	if (GameInstance)
	{
		AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(GameInstance->GetCurrentPlayerController());
		if (PC && PC->GetCurrentInputMode() != EInputMode::UI)
		{
			PC->GoToUIMode();
			UE_LOG(LogTemp, Log, TEXT("[BuildingManagePanelWidget] NativeOnActivated - Restored to UI mode"));
		}
	}

	// 비활성화 때 해제한 가림 고스트를 복귀 시 되살린다 — 카메라는 건드리지 않는다(프레이밍은 이미 맞다)
	if (bOwnsFocusTarget && Player && TargetBuilding)
	{
		Player->RestoreFocusTarget(TargetBuilding);
	}

	// 우측 컬럼(탭바+콘텐츠)만 아래에서 위로 등장 — 좌측 정보 패널은 손대지 않아 고정
	if (RightColumn)
	{
		bRightAppearing = true;
		RightAppearElapsed = 0.f;
		RightColumn->SetRenderTranslation(FVector2D(0.f, RightRiseDistance));
	}
}

void UBuildingManagePanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	UCGGameInstance* GameInstance = Cast<UCGGameInstance>(GetGameInstance());
	const AMainMapPlayerController* PlayerController = Cast<AMainMapPlayerController>(GameInstance->GetCurrentPlayerController());
	Player = Cast<APlayerCamera>(PlayerController->GetPawn());

	SetVisibility(ESlateVisibility::Visible);

	// 풀 재사용 인스턴스는 이전 세션의 TargetBuilding 을 들고 들어온다 — 소유권은 매 세션 SetTargetBuilding 이 다시 준다
	bOwnsFocusTarget = false;

	// 탭 버튼 그룹 초기화
	TabButtonGroup = NewObject<UCommonButtonGroupBase>(this);
	TabButtonGroup->SetSelectionRequired(true); // 항상 하나는 선택되어야 함

	// 탭 버튼들을 그룹에 등록 (순서가 탭 인덱스와 일치해야 함)
	// 래퍼는 UUserWidget 이라 그룹에 직접 못 넣고, 내부 UCommonButtonBase 핸들을 꺼내 등록
	auto RegisterTab = [this](UTabButtonWidget* Tab)
	{
		if (!Tab) return;
		if (UCommonButtonBase* Btn = Tab->GetButton())
		{
			TabButtonGroup->AddWidget(Btn);
			Btn->SetIsSelectable(true);
		}
	};
	RegisterTab(EnhancementTab);
	RegisterTab(TraitTab);
	RegisterTab(SkinTab);

	// 탭 선택 변경 이벤트 바인딩
	TabButtonGroup->OnSelectedButtonBaseChanged.AddDynamic(this, &UBuildingManagePanelWidget::OnTabSelectionChanged);

	// 스킨 탭 내부 외관/조명 토글 그룹 셋업 (ProductSellModal SortButtonGroup 패턴)
	SetupSkinSubTabGroup();

	// 특성 탭 정렬 토글 — 풀링 재사용 대비 매 NativeConstruct 재바인딩 (SkinSubTab 그룹과 동일 패턴, NativeDestruct 와 쌍)
	SetupTraitSortButtonGroup();

	// 닫기 버튼 바인딩
	if (UIE_CloseButton)
	{
		UIE_CloseButton->OnCloseClicked.AddDynamic(this, &UBuildingManagePanelWidget::OnCloseButtonClicked);
	}

	// 배경 클릭 시 닫기
	if (BackgroundBtn)
	{
		BackgroundBtn->OnClicked.AddDynamic(this, &UBuildingManagePanelWidget::OnBackgroundClicked);
	}

	// 레벨 배지 → 해금 로드맵 (NativeDestruct 와 쌍)
	if (LevelBadgeButton)
	{
		LevelBadgeButton->OnClicked.AddDynamic(this, &UBuildingManagePanelWidget::HandleLevelChipClicked);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildingManagePanel] LevelBadgeButton 미배치 — 레벨 로드맵 진입 불가 (WBP 확인)"));
	}

	// 상주 배지는 레벨업에 스스로 반응한다 (NativeDestruct 와 쌍)
	if (GameInstance)
	{
		if (USaveLoadManager* SaveMgr = GameInstance->GetSubsystem<USaveLoadManager>())
		{
		}
	}

	// [재배치] — 좌하단 정적 버튼 (클릭 사운드는 버튼이 자체 재생)
	if (RelocateButton)
	{
		RelocateButton->OnClicked().AddUObject(this, &UBuildingManagePanelWidget::HandleRelocateRequested);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildingManagePanel] RelocateButton 미배치 — 재배치 진입 불가 (WBP 확인)"));
	}

	// 강화 슬롯 버튼 바인딩은 RebuildEnhancementSlots() 내부에서 슬롯 생성과 동시에 수행.
	// (BuildingFloor는 Clicked 1회, 나머지는 Pressed/Released 홀드 — BindSlotButton 에서 분기)

	// 오피스 입장 버튼 바인딩 (공용 영역 - 직원/프로젝트 진행의 단일 진입점)
	if (EnterOfficeButton)
	{
		EnterOfficeButton->OnClicked().AddUObject(this, &UBuildingManagePanelWidget::OnEnterOfficeButtonClicked);
	}

	// 강화 배율 선택기 — OnModeChanged 구독 + 세션 모드 복원 (모드는 패널 세션 유지)
	if (BulkModeSelector)
	{
		BulkModeSelector->OnModeChanged.AddUObject(this, &UBuildingManagePanelWidget::ApplyBulkMode);
		BulkModeSelector->SetMode(BulkMode);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildingManagePanel] BulkModeSelector 미배치 — 강화 배율 비활성 (WBP 확인)"));
	}

	// 미션 가이드(M3 EnterOffice) — 관리 패널 열림 = 빌딩 클릭 → [입장] 유도 페이즈로 전환 + [입장] 버튼 하이라이트 등록
	if (UMissionManagerSubsystem* MissionMgr = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>())
	{
		bLastTutorialCompleted = MissionMgr->IsTutorialCompleted();
		MissionMgr->OnMissionCompleted.AddUObject(this, &UBuildingManagePanelWidget::HandleMissionCompleted);
		MissionMgr->NotifyManagePanelOpened(this);
	}

	// 초기 탭 설정 (강화 탭)
	if (TabButtonGroup && EnhancementTab)
	{
		TabButtonGroup->SelectButtonAtIndex(0);
	}
	// ContentSwitcher를 명시적으로 초기화 (델리게이트 미발동 대비)
	CurrentTab = EBuildingManageTab::Enhancement;
	if (ContentSwitcher)
	{
		ContentSwitcher->SetActiveWidgetIndex(0);
	}

	// 운영 업데이트 델리게이트 바인딩 (실시간 남은 시간 갱신)
	if (UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance()))
	{
		if (UProjectOperationManager* OpMgr = GI->GetSubsystem<UProjectOperationManager>())
		{
			OpMgr->OnOperationUpdated.AddDynamic(this, &UBuildingManagePanelWidget::HandleOperationUpdated);
			OpMgr->OnOperationCompleted.AddDynamic(this, &UBuildingManagePanelWidget::HandleOperationCompleted);

			// 기대 수익 변경 — 직원 배치/레벨업/강화 시 즉시 UI 반영
			OpMgr->OnExpectedRevenueChanged.AddUObject(this, &UBuildingManagePanelWidget::HandleExpectedRevenueChanged);
		}
	}

	// ===== 특성 슬롯/상세 팝업 델리게이트 바인딩 (인스턴스당 1회) =====
	if (!bTraitDelegatesBound)
	{
		if (UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance()))
		{
			if (UBuildingTraitManagerSubsystem* TraitMgr = GI->GetSubsystem<UBuildingTraitManagerSubsystem>())
			{
				TraitMgr->OnTraitSlotChanged.AddUObject(this, &UBuildingManagePanelWidget::HandleTraitSlotChanged);
				TraitMgr->OnTraitSlotUnlocked.AddUObject(this, &UBuildingManagePanelWidget::HandleTraitSlotUnlocked);
			}
		}

		// 팝업 델리게이트는 EnsureTraitDetailPopup() 가 생성 시점(첫 특성 클릭)에 바인딩 — NativeConstruct 시엔 팝업이 아직 없음

		bTraitDelegatesBound = true;
	}

	// 모달 레이어 크롬 (정적 트리 위젯 — Construct/Destruct 쌍)
	if (TraitModalDimButton) TraitModalDimButton->OnClicked.AddDynamic(this, &UBuildingManagePanelWidget::OnTraitModalDimClicked);
	if (TraitNavPrevButton) TraitNavPrevButton->OnClicked().AddUObject(this, &UBuildingManagePanelWidget::HandleTraitNavPrev);
	if (TraitNavNextButton) TraitNavNextButton->OnClicked().AddUObject(this, &UBuildingManagePanelWidget::HandleTraitNavNext);

	// 팝업/딤 초기 숨김
	CloseTraitDetailPopup();

	// 1분 주기 기대 수익 폴링 — 패널 열린 동안만 동작, 이벤트 델리게이트 누락 대비 fallback
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			ExpectedRevenuePollHandle,
			this,
			&UBuildingManagePanelWidget::OnExpectedRevenuePollTick,
			ExpectedRevenuePollIntervalSeconds,
			true);
	}

	// TargetBuilding이 이미 설정된 상태면 데이터 로드
	if (TargetBuilding)
	{
		RebuildEnhancementSlots();
		PopulateBuildingStats();
		UpdateProjectStatus();
	}
}

void UBuildingManagePanelWidget::NativeDestruct()
{
	// 운영 업데이트 델리게이트 해제
	if (UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance()))
	{
		if (UProjectOperationManager* OpMgr = GI->GetSubsystem<UProjectOperationManager>())
		{
			OpMgr->OnOperationUpdated.RemoveAll(this);
			OpMgr->OnOperationCompleted.RemoveAll(this);
			OpMgr->OnExpectedRevenueChanged.RemoveAll(this);
		}

		// 특성 매니저 델리게이트 해제
		if (UBuildingTraitManagerSubsystem* TraitMgr = GI->GetSubsystem<UBuildingTraitManagerSubsystem>())
		{
			TraitMgr->OnTraitSlotChanged.RemoveAll(this);
			TraitMgr->OnTraitSlotUnlocked.RemoveAll(this);
		}

		if (USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>())
		{
		}

		if (UMissionManagerSubsystem* MissionMgr = GI->GetSubsystem<UMissionManagerSubsystem>())
		{
			MissionMgr->OnMissionCompleted.RemoveAll(this);
		}
	}

	// 특성 상세 팝업/딤 언바인딩
	if (TraitDetailPopup)
	{
		TraitDetailPopup->OnEquipClicked.Unbind();
		TraitDetailPopup->OnUnequipClicked.Unbind();
		TraitDetailPopup->OnCloseClicked.Unbind();
	}
	if (TraitModalDimButton) TraitModalDimButton->OnClicked.RemoveDynamic(this, &UBuildingManagePanelWidget::OnTraitModalDimClicked);
	if (TraitNavPrevButton) TraitNavPrevButton->OnClicked().RemoveAll(this);
	if (TraitNavNextButton) TraitNavNextButton->OnClicked().RemoveAll(this);
	bTraitDelegatesBound = false;

	// 기대 수익 폴링 타이머 해제
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ExpectedRevenuePollHandle);
	}

	Super::NativeDestruct();

	// 홀드 타이머 정리
	StopEnhancementHold();

	// 버튼 언바인딩
	if (BackgroundBtn)
	{
		BackgroundBtn->OnClicked.RemoveDynamic(this, &UBuildingManagePanelWidget::OnBackgroundClicked);
	}

	if (RelocateButton)
	{
		RelocateButton->OnClicked().RemoveAll(this);
	}

	if (LevelBadgeButton)
	{
		LevelBadgeButton->OnClicked.RemoveDynamic(this, &UBuildingManagePanelWidget::HandleLevelChipClicked);
	}

	if (UIE_CloseButton)
	{
		UIE_CloseButton->OnCloseClicked.RemoveAll(this);
	}

	// TabButtonGroup 정리
	if (TabButtonGroup)
	{
		TabButtonGroup->OnSelectedButtonBaseChanged.RemoveAll(this);
	}

	// SkinSubTabGroup 정리 (각 버튼의 OnPressed 람다는 WeakLambda 패턴 아니므로 명시적 해제 필요)
	if (ExteriorBtn) { ExteriorBtn->OnPressed().RemoveAll(this); }
	if (LightingBtn) { LightingBtn->OnPressed().RemoveAll(this); }

	// TraitSortGroup 정리 — SetupTraitSortButtonGroup 에서 OnPressed().AddUObject 로 바인딩됨
	if (SortRarityBtn)   { SortRarityBtn->OnPressed().RemoveAll(this); }
	if (SortQuantityBtn) { SortQuantityBtn->OnPressed().RemoveAll(this); }
	if (SortNameBtn)     { SortNameBtn->OnPressed().RemoveAll(this); }


	// 오피스 입장 버튼 언바인딩
	if (EnterOfficeButton)
	{
		EnterOfficeButton->OnClicked().RemoveAll(this);
	}

	// 강화 배율 선택기 구독 해제
	if (BulkModeSelector)
	{
		BulkModeSelector->OnModeChanged.RemoveAll(this);
	}

	// 동적 생성된 모든 UpgradeSlot의 버튼 언바인딩 (RemoveAll(this)로 일괄 해제)
	for (const TPair<EBuildingEnhancementType, UUpgradeSlot*>& Pair : DynamicSlots)
	{
		if (UUpgradeSlot* SlotItem = Pair.Value)
		{
			if (UCostActionButtonWidget* Btn = SlotItem->GetUpgradeButton())
			{
				Btn->OnClicked().RemoveAll(this);
				Btn->OnPressed().RemoveAll(this);
				Btn->OnReleased().RemoveAll(this);
			}
		}
	}
}

void UBuildingManagePanelWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshEnhancementLocksIfTutorialStateChanged();

	// 우측 컬럼 등장 슬라이드 (CubicOut, 위치만 — 투명도 페이드는 스택 FADE_ONLY 담당)
	if (bRightAppearing && RightColumn)
	{
		RightAppearElapsed += InDeltaTime;
		const float T = (RightAppearDuration > 0.f) ? FMath::Clamp(RightAppearElapsed / RightAppearDuration, 0.f, 1.f) : 1.f;
		const float Eased = 1.f - FMath::Pow(1.f - T, 3.f);
		RightColumn->SetRenderTranslation(FVector2D(0.f, RightRiseDistance * (1.f - Eased)));
		if (T >= 1.f)
		{
			bRightAppearing = false;
			RightColumn->SetRenderTranslation(FVector2D::ZeroVector);
		}
	}

	// 건물이 파괴되었거나 없어진 경우 위젯 닫기
	if (!TargetBuilding || !TargetBuilding->IsValidLowLevel())
	{
		OnCloseButtonClicked();
	}
}

void UBuildingManagePanelWidget::NativeOnDeactivated()
{
	Super::NativeOnDeactivated();

	// 우측 컬럼 트윈 잔여 상태 정리 (풀 재사용 시 오프셋 박제 방지)
	bRightAppearing = false;
	if (RightColumn)
	{
		RightColumn->SetRenderTranslation(FVector2D::ZeroVector);
	}

	// 패널이 닫히면 어떤 모뉴먼트도 선택 상태가 아니므로 모든 영향권 링 + 바닥 하이라이트를 숨김
	if (UWorld* World = GetWorld())
	{
		if (UEntityManager* EntityMgr = World->GetSubsystem<UEntityManager>())
		{
			for (ABuildingBaseActor* BuildingActor : EntityMgr->GetBuildings())
			{
				if (BuildingActor)
				{
					BuildingActor->SetKeystoneRingVisible(false);
					BuildingActor->SetAuraHighlight(false);
				}
			}
		}
	}

	// 키스톤 와이드 포커스로 임시 확장했던 줌 상한 원복 + 가림 고스트 해제 (복귀는 NativeOnActivated 가 재등록)
	if (Player)
	{
		Player->RestoreZoomRange();
		Player->ClearFocusTarget();
	}

	// 특성 상세 팝업 닫기 + 타겟 슬롯 초기화 (재활성 시 깨끗한 상태)
	CloseTraitDetailPopup();
	SetPendingTargetSlot(INDEX_NONE);

	// 선택 셰브론 마커 해제
	if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
	{
		if (UInGameLayerWidget* InGame = UIMgr->GetInGameLayer())
		{
			InGame->SetSelectedBuildingMarker(nullptr);
		}
	}

	// 패널 종료 시 InGameLayer가 즉시 조명/버블 상태를 재평가하도록 broadcast
	// (조명 프리뷰로 머티리얼이 켜진 상태에서 idle 빌딩을 다시 어둡게 동기화)
	if (TargetBuilding)
	{
		TargetBuilding->OnBubbleRefreshRequested.Broadcast(TargetBuilding->GetBuildingIndex());
	}

	// UI가 비활성화될 때 Normal 모드로 복원
	// (ESC 키, 다른 UI 열기 등으로 닫힐 때 자동으로 호출됨)
	UCGGameInstance* GameInstance = Cast<UCGGameInstance>(GetGameInstance());
	if (GameInstance)
	{
		AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(GameInstance->GetCurrentPlayerController());
		if (PC && PC->GetCurrentInputMode() == EInputMode::UI)
		{
			PC->GoToNormalMode();
			UE_LOG(LogTemp, Log, TEXT("[BuildingManagePanelWidget] NativeOnDeactivated - Restored to Normal mode"));
		}
	}
}

void UBuildingManagePanelWidget::SetTargetBuilding(ABuildingBaseActor* Building, EBuildingManageTab InitialTab)
{
	TargetBuilding = Building;

	// 모뉴먼트(키스톤)는 직원/특성 루프 제외 → [강화][스킨]만 노출, [특성] 탭 숨김
	const bool bKeystone = TargetBuilding && TargetBuilding->IsKeystoneMonument();

	// 영향권 링/하이라이트는 선택된 모뉴먼트에만 표시 — 먼저 전체 링 + 바닥 하이라이트를 끄고, 대상이 모뉴먼트면 그 링 + 영향권 멤버 하이라이트를 켠다
	if (UWorld* World = GetWorld())
	{
		if (UEntityManager* EntityMgr = World->GetSubsystem<UEntityManager>())
		{
			for (ABuildingBaseActor* BuildingActor : EntityMgr->GetBuildings())
			{
				if (BuildingActor)
				{
					BuildingActor->SetKeystoneRingVisible(false);
					BuildingActor->SetAuraHighlight(false);
				}
			}
		}

		// 대상이 모뉴먼트면 영향권에 드는 이웃 빌딩들에 파란 바닥 하이라이트
		if (bKeystone)
		{
			if (UKeystoneAuraSubsystem* AuraSys = World->GetSubsystem<UKeystoneAuraSubsystem>())
			{
				for (ABuildingBaseActor* Member : AuraSys->GetBuildingsInKeystoneZone(TargetBuilding))
				{
					if (Member)
					{
						Member->SetAuraHighlight(true);
					}
				}
			}
		}
	}
	if (bKeystone)
	{
		TargetBuilding->SetKeystoneRingVisible(true);
	}

	// 입장(오피스) 버튼은 모뉴먼트엔 의미 없음(직원 미수용) → 숨김
	if (UWidget* EnterBtn = GetWidgetFromName(TEXT("EnterOfficeButton")))
	{
		EnterBtn->SetVisibility(bKeystone ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}

	if (TraitTab)
	{
		TraitTab->SetVisibility(bKeystone ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	if (bKeystone && CurrentTab == EBuildingManageTab::Trait && TabButtonGroup)
	{
		TabButtonGroup->SelectButtonAtIndex(0);  // 숨겨진 특성 탭에 머물지 않도록 강화로 강제
	}

	// 선택 셰브론 마커 (단일 슬롯이라 대상 교체 시 자동 대체)
	if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
	{
		if (UInGameLayerWidget* InGame = UIMgr->GetInGameLayer())
		{
			InGame->SetSelectedBuildingMarker(TargetBuilding);
		}
	}

	// GameInstance에 현재 관리 중인 건물 등록
	if (UCGGameInstance* GameInstance = Cast<UCGGameInstance>(GetGameInstance()))
	{
		GameInstance->SetCurrentManagedBuilding(TargetBuilding);
	}

	// 카메라 프레이밍 요청 지점 (우측 패널 회피 → 건물은 화면 왼쪽)
	if (Player && TargetBuilding)
	{
		// 층수 비례 포커싱. 키스톤은 영향권 반경이 보이게 더 넓게 (화면 왼쪽 1/4)
		const float AuraRadius = TargetBuilding->IsKeystoneMonument() ? TargetBuilding->GetKeystoneAuraRadiusCm() : 0.f;
		Player->FocusOnBuildingOrAura(TargetBuilding, CameraFocusTopPadding, CameraFocusBottomPadding, 0.3f, AuraRadius);
		bOwnsFocusTarget = true;
	}

	// 건물이 설정되면 데이터 로드
	if (TargetBuilding)
	{
		RebuildEnhancementSlots();
		PopulateBuildingStats();
		UpdateProjectStatus();

		// 좌측 패널 건물 아이콘 로드
		if (EntityImage)
		{
			UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
			FBuildableCardTable BuildableInfo;
			if (TableMgr && TableMgr->GetBuildableInfo(TargetBuilding->GetBuildingID(), BuildableInfo))
			{
				const FSoftObjectPath IconPath = BuildableInfo.UIIcon.ToSoftObjectPath();
				if (IconPath.IsValid())
				{
					UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(
						IconPath,
						FStreamableDelegate::CreateWeakLambda(this, [this, IconPath]()
						{
							UTexture2D* Texture = Cast<UTexture2D>(IconPath.ResolveObject());
							if (Texture && EntityImage)
							{
								EntityImage->SetBrushFromTexture(Texture);
							}
						})
					);
				}
			}
		}
	}

	// 초기 탭 강제 — 딥링크(예: 금고 강화 유도)와 풀링 stale 리셋을 함께 담당.
	// NativeConstruct 의 탭 세팅은 최초 1회뿐이고 재사용 인스턴스는 유저가 마지막에 보던 탭이 남는다
	// (726행 "매 셋업 기본값 리셋" 원칙과 동일 뿌리). 여긴 여는 쪽이 의도를 주입하는 1회성 데이터
	// 셋업이라 NativeOnActivated 탭 조작 금지 규칙과 무관하다.
	EBuildingManageTab ResolvedTab = InitialTab;
	if (bKeystone && ResolvedTab == EBuildingManageTab::Trait)
	{
		ResolvedTab = EBuildingManageTab::Enhancement;  // 키스톤은 특성 탭 숨김 → 강화로
	}
	const int32 TabIndex = static_cast<int32>(ResolvedTab);
	if (TabButtonGroup)
	{
		TabButtonGroup->SelectButtonAtIndex(TabIndex);
	}
	CurrentTab = ResolvedTab;
	if (ContentSwitcher)
	{
		ContentSwitcher->SetActiveWidgetIndex(TabIndex);
	}
}

void UBuildingManagePanelWidget::OnCloseButtonClicked()
{
	UE_LOG(LogTemp, Log, TEXT("[BuildingManagePanelWidget] Back button clicked"));

	if (Player)
	{
		Player->EndBuildingRelocation();
	}

	CloseWithAnimation();
}

void UBuildingManagePanelWidget::HandleRelocateRequested()
{
	ABuildingBaseActor* Building = TargetBuilding;
	APlayerCamera* PlayerCam = Player;
	if (!Building || !PlayerCam)
	{
		return;
	}

	// 클릭 사운드는 RelocateButton(CommonButtonBase)이 자체 재생 — 여기서 중복 재생 금지

	// 스택 규칙: 닫기 먼저(마커 해제 + UI→Normal 복원), push 나중
	DeactivateWidget();

	PlayerCam->StartBuildingRelocation(Building);

	// BuildPlacement 하단 확인/취소 패널 — 구 롱프레스 경로 이식
	UCGGameInstance* GameInstance = Cast<UCGGameInstance>(GetGameInstance());
	if (!GameInstance)
	{
		return;
	}
	UTableManagerSubsystem* TableManager = GameInstance->GetSubsystem<UTableManagerSubsystem>();
	UUIManagerSubsystem* UIManager = GameInstance->GetSubsystem<UUIManagerSubsystem>();
	if (!TableManager || !UIManager)
	{
		return;
	}
	TSubclassOf<UUserWidget> BuildPlacementClass = TableManager->GetWidgetClass(EWidgetType::BuildPlacementPanel);
	UUIBase* UIBase = UIManager->GetUIBase();
	if (!BuildPlacementClass || !UIBase)
	{
		return;
	}
	if (UBuildPlacementPanelWidget* PlacementWidget =
		Cast<UBuildPlacementPanelWidget>(UIBase->PushBottomClass(BuildPlacementClass.Get())))
	{
		PlacementWidget->SetBuildableInfo(FBuildableCardTable(), true);
	}
}

void UBuildingManagePanelWidget::OnBackgroundClicked()
{
	OnCloseButtonClicked();
}

void UBuildingManagePanelWidget::OnTabSelectionChanged(UCommonButtonBase* SelectedButton, int32 ButtonIndex)
{
	// 사운드는 이 핸들러에만 — SwitchToTab 은 초기화/내부 경로에서도 불림
	if (USoundManagerSubsystem* SM = GetGameInstance()->GetSubsystem<USoundManagerSubsystem>())
	{
		SM->PlayUISound(CGUISoundTags::TabSwitch);
	}

	// ButtonIndex가 탭 인덱스와 일치 (등록 순서대로)
	EBuildingManageTab NewTab = static_cast<EBuildingManageTab>(ButtonIndex);

	UE_LOG(LogTemp, Log, TEXT("[BuildingManagePanelWidget] Tab selection changed to index %d"), ButtonIndex);

	SwitchToTab(NewTab);

	// M11 EquipFirstTrait — 특성 탭 진입 시 OpenTraitTab→UnlockSlot 페이즈 전이
	if (NewTab == EBuildingManageTab::Trait)
	{
		if (UMissionManagerSubsystem* M = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>())
		{
			M->NotifyTraitTabOpened();
		}
	}
}

void UBuildingManagePanelWidget::SwitchToTab(EBuildingManageTab NewTab)
{
	if (!ContentSwitcher)
	{
		UE_LOG(LogTemp, Error, TEXT("[BuildingManagePanelWidget] ContentSwitcher is null!"));
		return;
	}

	// 현재 탭 업데이트
	CurrentTab = NewTab;

	// 배율 선택기는 강화 탭 전용 (패널 왼쪽 바깥 하단 부유)
	if (BulkModeSelector)
	{
		BulkModeSelector->SetVisibility(NewTab == EBuildingManageTab::Enhancement
			? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	// 어느 탭으로 가든 특성 상세 팝업은 닫는다 (팝업이 좌측 컬럼 공용이라 탭과 분리 — Trait 외 탭은 LoadTraitTabContent 를 안 타므로 여기서 명시적으로)
	CloseTraitDetailPopup();

	// Widget Switcher의 활성 인덱스 변경
	ContentSwitcher->SetActiveWidgetIndex(static_cast<int32>(NewTab));

	// 탭별 특수 로직
	switch (NewTab)
	{
	case EBuildingManageTab::Enhancement:
		UE_LOG(LogTemp, Log, TEXT("[BuildingManagePanelWidget] Switched to Enhancement tab"));
		RebuildEnhancementSlots();
		break;

	case EBuildingManageTab::Trait:
		UE_LOG(LogTemp, Log, TEXT("[BuildingManagePanelWidget] Switched to Trait tab"));
		LoadTraitTabContent();
		break;

	case EBuildingManageTab::Skin:
		UE_LOG(LogTemp, Log, TEXT("[BuildingManagePanelWidget] Switched to Skin tab"));
		// 스킨 탭 활성화 시 외관/조명 카드 모두 로드 (서로 다른 컨테이너이므로 충돌 없음)
		LoadAvailableSkins();
		LoadAvailableLights();
		break;
	}
}

void UBuildingManagePanelWidget::OnBuildingUpgradeButtonClicked()
{
	UE_LOG(LogTemp, Log, TEXT("[BuildingManagePanelWidget] Building floor upgrade button clicked"));

	if (!TargetBuilding)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildingManagePanelWidget] TargetBuilding is null"));
		return;
	}
	if (!IsEnhancementInteractionAllowed(EBuildingEnhancementType::BuildingFloor))
	{
		return;
	}

	// 건물 층수 업그레이드 (UpgradeEnhancement로 통합)
	bool bSuccess = TargetBuilding->UpgradeEnhancement(EBuildingEnhancementType::BuildingFloor, 1);

	if (bSuccess)
	{
		UE_LOG(LogTemp, Log, TEXT("[BuildingManagePanelWidget] Building floor upgraded"));

		// UI 업데이트 — DynamicSlots 에서 BuildingFloor 슬롯 찾아 갱신 + 시각 이펙트만 (사운드는 AddFloor 의 Drop thud 가 책임, 중복 방지)
		UUpgradeSlot* FloorSlot = DynamicSlots.FindRef(EBuildingEnhancementType::BuildingFloor);
		if (FloorSlot)
		{
			UpdateEnhancementSlot(EBuildingEnhancementType::BuildingFloor, FloorSlot);
			FloorSlot->PlayUpgradeEffect(false);
		}

		// 건물이 높아졌으므로 카메라 다시 포커싱 (키스톤은 층수 강화로 영향권도 커지므로 더 큰 줌을 반영)
		if (Player)
		{
			Player->StopCameraTransition();
			const float AuraRadius = TargetBuilding->IsKeystoneMonument() ? TargetBuilding->GetKeystoneAuraRadiusCm() : 0.f;
			Player->FocusOnBuildingOrAura(TargetBuilding, CameraFocusTopPadding, CameraFocusBottomPadding, 0.3f, AuraRadius);
		}

		// 좌측 패널 건물 정보 갱신
		PopulateBuildingStats();

		// 미션판 G1 RaiseBuildingFloor 진행은 성공한 빌드업 횟수만 센다.
		if (UMissionManagerSubsystem* M = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>())
		{
			M->NotifyBuildingFloorUpgraded();
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildingManagePanelWidget] Failed to upgrade building floor"));
		if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
		{
			// 빌드업은 벽돌로 결제(DT) — 실패는 벽돌 부족. 자금 부족 문구는 오해 소지.
			UIMgr->ShowNotification(NSLOCTEXT("Notification", "NotEnoughBricks", "벽돌이 부족합니다"), 3.0f, ENotificationType::Failed);
		}
	}
}

// ========== 특성 탭 관련 함수들 (BUILDING_TRAIT_SYSTEM v1.1) ==========

void UBuildingManagePanelWidget::LoadTraitTabContent()
{
	if (!TargetBuilding)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildingManagePanelWidget] LoadTraitTabContent — TargetBuilding NULL"));
		return;
	}

	// 탭 재진입 시 상세 팝업 닫고 타겟 슬롯 초기화
	CloseTraitDetailPopup();
	SetPendingTargetSlot(INDEX_NONE);

	RebuildTraitSlots();
	RebuildTraitCardList();
	RefreshSetBonusBand();
}

void UBuildingManagePanelWidget::SetupTraitSortButtonGroup()
{
	// 풀링 인스턴스 재사용 시 이전 정렬 상태 잔존 → SelectButtonAtIndex(0)(등급순)과 어긋남. 매 셋업 기본값 리셋.
	TraitSortMode = ETraitSortMode::Rarity;
	bTraitSortAscending = false;

	TraitSortGroup = NewObject<UCommonButtonGroupBase>(this);
	if (!TraitSortGroup) return;

	TraitSortGroup->SetSelectionRequired(true);

	auto Setup = [this](UButtonWidget* Btn, void (UBuildingManagePanelWidget::*Handler)())
	{
		if (!Btn) return;
		Btn->SetIsSelectable(true);
		Btn->SetIsInteractableWhenSelected(true);
		TraitSortGroup->AddWidget(Btn);
		Btn->OnPressed().AddUObject(this, Handler);
	};
	Setup(SortRarityBtn,   &UBuildingManagePanelWidget::HandleSortRarityClicked);
	Setup(SortQuantityBtn, &UBuildingManagePanelWidget::HandleSortQuantityClicked);
	Setup(SortNameBtn,     &UBuildingManagePanelWidget::HandleSortNameClicked);

	TraitSortGroup->SelectButtonAtIndex(0);  // Rarity 기본
	UpdateTraitSortButtonTexts();
}

void UBuildingManagePanelWidget::HandleSortRarityClicked()
{
	if (TraitSortMode == ETraitSortMode::Rarity) { bTraitSortAscending = !bTraitSortAscending; }
	else { TraitSortMode = ETraitSortMode::Rarity; bTraitSortAscending = false; }
	UpdateTraitSortButtonTexts();
	RebuildTraitCardList();
}

void UBuildingManagePanelWidget::HandleSortQuantityClicked()
{
	if (TraitSortMode == ETraitSortMode::Quantity) { bTraitSortAscending = !bTraitSortAscending; }
	else { TraitSortMode = ETraitSortMode::Quantity; bTraitSortAscending = false; }
	UpdateTraitSortButtonTexts();
	RebuildTraitCardList();
}

void UBuildingManagePanelWidget::HandleSortNameClicked()
{
	if (TraitSortMode == ETraitSortMode::Name) { bTraitSortAscending = !bTraitSortAscending; }
	else { TraitSortMode = ETraitSortMode::Name; bTraitSortAscending = false; }
	UpdateTraitSortButtonTexts();
	RebuildTraitCardList();
}

void UBuildingManagePanelWidget::UpdateTraitSortButtonTexts()
{
	auto ApplyText = [](UButtonWidget* Btn, bool bIsSelected, bool bAsc, const FString& Label)
	{
		if (!Btn) return;
		const FString Suffix = bIsSelected ? (bAsc ? FString(TEXT(" ▲")) : FString(TEXT(" ▼"))) : FString();
		Btn->SetButtonText(FText::FromString(Label + Suffix));
		Btn->SetLetterSpacing(-3);
	};

	ApplyText(SortRarityBtn,   TraitSortMode == ETraitSortMode::Rarity,   bTraitSortAscending, TEXT("등급순"));
	ApplyText(SortQuantityBtn, TraitSortMode == ETraitSortMode::Quantity, bTraitSortAscending, TEXT("보유순"));
	ApplyText(SortNameBtn,     TraitSortMode == ETraitSortMode::Name,     bTraitSortAscending, TEXT("이름순"));
}

void UBuildingManagePanelWidget::RebuildTraitCardList()
{
	if (!TraitCardContainer) return;

	UBuildingTraitManagerSubsystem* TraitMgr = GetGameInstance()->GetSubsystem<UBuildingTraitManagerSubsystem>();
	UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	if (!TraitMgr || !TableMgr) return;

	// 기존 카드 제거
	TraitCardContainer->ClearChildren();
	SpawnedTraitCards.Reset();

	// 위젯 클래스 로드 (DT_WidgetClass에 UIE_TraitCard_QtyBelow 등록 필요)
	// 일단은 ConstructorHelpers 패턴 우회 위해 EWidgetType 미정 — WBP 디자이너 카드 첫 인스턴스로부터 클래스 추출
	// 임시: SoftClassPath 로드. 추후 EWidgetType::TraitCardQtyBelow 추가.
	UClass* CardClass = LoadClass<UTraitCardSlotWidget>(nullptr,
		TEXT("/Game/CompanyGrowth/UI/Elements/Cards/UIE_TraitCard_QtyBelow.UIE_TraitCard_QtyBelow_C"));
	if (!CardClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildingManagePanelWidget] UIE_TraitCard_QtyBelow 클래스 로드 실패"));
		return;
	}

	struct FTraitEntry
	{
		FName TraitID;
		int32 TotalCount;
		int32 EquippedCount;
		FBuildingTraitTableRow Row;
	};

	// 보유 + "현 건물" 장착 합산 ― 장착하면 인벤 맵에서 빠져 카드가 사라지던 문제 해결.
	// EquippedCount 는 현 건물 기준 (카드 장착 칩 = 이 건물에 꽂힌 수).
	TMap<FName, int32> EquippedCountMap;
	if (TargetBuilding)
	{
		for (const FName& EqID : TraitMgr->GetEquippedTraits(TargetBuilding->GetBuildingIndex()))
		{
			if (!EqID.IsNone()) EquippedCountMap.FindOrAdd(EqID) += 1;
		}
	}

	TArray<FTraitEntry> Entries;
	const TMap<FName, int32>& OwnedMap = TraitMgr->GetAllOwnedTraits();
	for (const TPair<FName, int32>& Pair : OwnedMap)
	{
		FTraitEntry Entry;
		Entry.TraitID = Pair.Key;
		if (!TableMgr->GetBuildingTraitData(Entry.TraitID, Entry.Row)) continue;
		Entry.EquippedCount = EquippedCountMap.FindRef(Pair.Key);
		EquippedCountMap.Remove(Pair.Key);
		Entry.TotalCount = Pair.Value + Entry.EquippedCount;
		Entries.Add(Entry);
	}
	// 인벤 0 + 이 건물 장착만 있는 특성도 카드로 노출
	for (const TPair<FName, int32>& Pair : EquippedCountMap)
	{
		FTraitEntry Entry;
		Entry.TraitID = Pair.Key;
		if (!TableMgr->GetBuildingTraitData(Entry.TraitID, Entry.Row)) continue;
		Entry.EquippedCount = Pair.Value;
		Entry.TotalCount = Pair.Value;
		Entries.Add(Entry);
	}

	// 정렬
	const bool bAsc = bTraitSortAscending;
	Entries.Sort([this, bAsc](const FTraitEntry& A, const FTraitEntry& B)
	{
		switch (TraitSortMode)
		{
		case ETraitSortMode::Rarity:
			if (A.Row.Rarity != B.Row.Rarity)
				return bAsc ? (A.Row.Rarity < B.Row.Rarity) : (A.Row.Rarity > B.Row.Rarity);
			return A.Row.DisplayName.ToString() < B.Row.DisplayName.ToString();

		case ETraitSortMode::Quantity:
			if (A.TotalCount != B.TotalCount)
				return bAsc ? (A.TotalCount < B.TotalCount) : (A.TotalCount > B.TotalCount);
			return A.Row.DisplayName.ToString() < B.Row.DisplayName.ToString();

		case ETraitSortMode::Name:
		default:
			{
				const FString An = A.Row.DisplayName.ToString();
				const FString Bn = B.Row.DisplayName.ToString();
				return bAsc ? (An < Bn) : (An > Bn);
			}
		}
	});

	// 카드 생성
	for (const FTraitEntry& Entry : Entries)
	{
		UTraitCardSlotWidget* Card = CreateWidget<UTraitCardSlotWidget>(this, CardClass);
		if (!Card) continue;

		Card->SetTraitData(Entry.TraitID, Entry.TotalCount - Entry.EquippedCount, Entry.EquippedCount);

		// 카드 클릭 → 상세 팝업 (전달 FName = SetItemID 로 박은 TraitID)
		Card->OnItemCardClicked.BindUObject(this, &UBuildingManagePanelWidget::HandleTraitCardClicked);

		TraitCardContainer->AddChild(Card);
		SpawnedTraitCards.Add(Card);
	}

	// 보유 카운트 + 빈 상태 토글
	if (TraitCountText)
	{
		TraitCountText->SetText(FText::FromString(FString::Printf(TEXT("%d종"), Entries.Num())));
	}
	if (TraitEmptyStateBox)
	{
		TraitEmptyStateBox->SetVisibility(Entries.IsEmpty() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	UE_LOG(LogTemp, Log, TEXT("[BuildingManagePanelWidget] Trait cards rebuilt: %d (Mode=%d, Asc=%d)"),
		Entries.Num(), static_cast<int32>(TraitSortMode), bAsc ? 1 : 0);
}

// ========== 특성 슬롯 row / 상세 팝업 / 장착 플로우 ==========

void UBuildingManagePanelWidget::RebuildTraitSlots()
{
	if (!TraitSlotRow || !TargetBuilding) return;

	UBuildingTraitManagerSubsystem* TraitMgr = GetGameInstance()->GetSubsystem<UBuildingTraitManagerSubsystem>();
	if (!TraitMgr) return;

	// 슬롯은 capacity 고정(3) → WBP 에 정적 배치된 UIE_TraitSlot 을 그대로 사용. 생성/삭제 없이 상태만 갱신.
	// 자식 수집 + 클릭 바인딩은 1회만 (이후 호출은 reconfigure).
	if (SpawnedTraitSlots.Num() == 0)
	{
		const int32 NumChildren = TraitSlotRow->GetChildrenCount();
		for (int32 i = 0; i < NumChildren; ++i)
		{
			UTraitSlotWidget* SlotWidget = Cast<UTraitSlotWidget>(TraitSlotRow->GetChildAt(i));
			if (!SlotWidget) continue;

			SlotWidget->SetSlotIndex(SpawnedTraitSlots.Num());  // 자식 순서 = 슬롯 인덱스(좌→우)
			SlotWidget->OnSlotClicked.BindUObject(this, &UBuildingManagePanelWidget::HandleTraitSlotClicked);
			SpawnedTraitSlots.Add(SlotWidget);
		}
	}

	// 매 호출: 잠금/장착/빈칸 + 선택 상태만 반영
	const int32 BIdx = TargetBuilding->GetBuildingIndex();
	const TArray<FName> Equipped = TraitMgr->GetEquippedTraits(BIdx);

	for (int32 i = 0; i < SpawnedTraitSlots.Num(); ++i)
	{
		UTraitSlotWidget* SlotWidget = SpawnedTraitSlots[i];
		if (!SlotWidget) continue;

		if (!TraitMgr->IsSlotUnlocked(BIdx, i))
		{
			SlotWidget->SetLocked(UBuildingTraitManagerSubsystem::GetDiamondCostForSlot(i));
		}
		else if (Equipped.IsValidIndex(i) && !Equipped[i].IsNone())
		{
			SlotWidget->SetEquipped(Equipped[i]);
		}
		else
		{
			SlotWidget->SetEmpty();
		}

		SlotWidget->SetSelected(i == PendingTargetSlotIndex);
	}

	if (TraitSlotCountText)
	{
		TraitSlotCountText->SetText(FText::FromString(FString::Printf(TEXT("슬롯 %d/%d 해금"),
			TraitMgr->GetUnlockedSlotCount(BIdx), UBuildingTraitManagerSubsystem::GetMaxSlotCount())));
	}

	// 빈(해금) 슬롯이 있으면 첫 빈 슬롯을 기본 타겟으로 — 현재 타겟이 유효한 빈 슬롯이 아닐 때만 (사용자 명시 선택은 보존)
	const bool bTargetValidEmpty = Equipped.IsValidIndex(PendingTargetSlotIndex)
		&& Equipped[PendingTargetSlotIndex].IsNone()
		&& TraitMgr->IsSlotUnlocked(BIdx, PendingTargetSlotIndex);
	if (!bTargetValidEmpty)
	{
		SetPendingTargetSlot(FindFirstEmptyUnlockedSlot());
	}
}

void UBuildingManagePanelWidget::RefreshSetBonusBand()
{
	if (!SetBonusBand || !TargetBuilding) return;
	UBuildingTraitManagerSubsystem* TraitMgr = GetGameInstance()->GetSubsystem<UBuildingTraitManagerSubsystem>();
	UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	if (!TraitMgr || !TableMgr) return;

	SetBonusBand->ClearChildren();
	SpawnedSetChips.Reset();

	const TArray<FActiveBuildingTraitSetBonus> Bonuses = TraitMgr->CalculateSetBonuses(TargetBuilding->GetBuildingIndex());
	if (Bonuses.IsEmpty())
	{
		SetBonusBand->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	SetBonusBand->SetVisibility(ESlateVisibility::HitTestInvisible);

	TSubclassOf<UUserWidget> ChipClass = TableMgr->GetWidgetClass(EWidgetType::TraitSetChip);
	if (!ChipClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildingManagePanelWidget] TraitSetChip 위젯 클래스 미등록 (DT_WidgetClass 확인)"));
		return;
	}

	for (const FActiveBuildingTraitSetBonus& Bonus : Bonuses)
	{
		UTraitSetChipWidget* Chip = CreateWidget<UTraitSetChipWidget>(this, ChipClass);
		if (!Chip) continue;

		// 표시명 SOT = BuildingTraitCategory.h (⚠ UMETA 는 에디터 전용 — 패키징 빌드에서 "Revenue"/"Efficiency" 로 떨어진다)
		const FText CatName = GetBuildingTraitCategoryDisplayName(Bonus.Category);
		const FText Label = Bonus.bActive
			? FText::Format(NSLOCTEXT("TraitSet", "ChipActive", "{0} {1}세트 ― {2}"),
				CatName, FText::AsNumber(Bonus.RequiredCount), Bonus.BonusDescription)
			: FText::Format(NSLOCTEXT("TraitSet", "ChipProgress", "{0} {1}/{2}"),
				CatName, FText::AsNumber(Bonus.CountInBuilding), FText::AsNumber(Bonus.RequiredCount));
		Chip->SetChipData(Label, Bonus.bActive);

		if (UHorizontalBoxSlot* BoxSlot = SetBonusBand->AddChildToHorizontalBox(Chip))
		{
			BoxSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
		}
		SpawnedSetChips.Add(Chip);
	}
}

void UBuildingManagePanelWidget::HandleTraitSlotClicked(int32 SlotIndex)
{
	if (!TargetBuilding) return;
	UBuildingTraitManagerSubsystem* TraitMgr = GetGameInstance()->GetSubsystem<UBuildingTraitManagerSubsystem>();
	if (!TraitMgr) return;

	const int32 BIdx = TargetBuilding->GetBuildingIndex();

	// 잠긴 슬롯 → 다이아 충분 시 개방 확인 다이얼로그 / 부족 시 Toast (즉시 차감 금지)
	if (!TraitMgr->IsSlotUnlocked(BIdx, SlotIndex))
	{
		const int32 Cost = UBuildingTraitManagerSubsystem::GetDiamondCostForSlot(SlotIndex);
		UResourceItemManager* ResMgr = GetGameInstance()->GetSubsystem<UResourceItemManager>();
		const int64 Have = ResMgr ? ResMgr->GetResourceAmount(EResourceType::Diamond) : 0;

		if (Have < Cost)
		{
			if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
			{
				UIMgr->ShowNotification(NSLOCTEXT("Notification", "NotEnoughDiamond", "보석이 부족합니다"), 3.0f, ENotificationType::Failed);
			}
		}
		else
		{
			ShowSlotUnlockConfirm(SlotIndex, Cost);
		}
		return;
	}

	const TArray<FName> Equipped = TraitMgr->GetEquippedTraits(BIdx);
	const bool bOccupied = Equipped.IsValidIndex(SlotIndex) && !Equipped[SlotIndex].IsNone();

	// 이 슬롯을 장착 타겟으로 지정 (다음 [장착] 이 여기로)
	SetPendingTargetSlot(SlotIndex);

	if (bOccupied)
	{
		// 장착된 특성 상세 (해제/교체 모드)
		OpenTraitDetailForSlot(SlotIndex);
	}
	else
	{
		// 빈 슬롯: 타겟만 지정, 인벤토리 카드 선택 대기 (팝업은 닫아둠)
		CloseTraitDetailPopup();
	}
}

void UBuildingManagePanelWidget::ShowSlotUnlockConfirm(int32 SlotIndex, int32 DiamondCost)
{
	UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
	UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	if (!UIMgr || !UIMgr->GetUIBase() || !TableMgr) return;

	TSubclassOf<UUserWidget> Cls = TableMgr->GetWidgetClass(EWidgetType::ConfirmCancel);
	if (!Cls) return;

	UConfirmCancelWidget* Confirm = Cast<UConfirmCancelWidget>(UIMgr->GetUIBase()->PushPromptClass(Cls.Get()));
	if (!Confirm) return;

	// 프롬프트 스택은 위젯 풀 재사용 — 이전 소비자(운영 종료/부지 인수 등)의 잔류 바인딩 해제 후 소유
	// (해제 없으면 재사용 인스턴스에 페이로드 다른 HandleSlotUnlockConfirmed 가 누적 → 확인 1회에 슬롯 N개 차감)
	Confirm->OnConfirm.Clear();
	Confirm->OnCancel.Clear();

	Confirm->SetTitle(NSLOCTEXT("TraitSlot", "UnlockTitle", "특성 슬롯 개방"));
	Confirm->SetMessage(FText::Format(
		NSLOCTEXT("TraitSlot", "UnlockMsg", "보석 {0}개로 이 슬롯을 개방할까요?"),
		FText::AsNumber(DiamondCost)));
	Confirm->SetConfirmButtonText(NSLOCTEXT("TraitSlot", "UnlockConfirm", "개방"));
	Confirm->SetCancelButtonText(NSLOCTEXT("TraitSlot", "Cancel", "취소"));

	// 확인 시에만 실제 차감 (성공 → OnTraitSlotUnlocked → RebuildTraitSlots). 다이얼로그는 bAutoRemove 로 자동 닫힘.
	Confirm->OnConfirm.AddUObject(this, &UBuildingManagePanelWidget::HandleSlotUnlockConfirmed, SlotIndex);
}

void UBuildingManagePanelWidget::HandleSlotUnlockConfirmed(int32 SlotIndex)
{
	if (!TargetBuilding) return;
	UBuildingTraitManagerSubsystem* TraitMgr = GetGameInstance()->GetSubsystem<UBuildingTraitManagerSubsystem>();
	if (!TraitMgr) return;

	const bool bUnlocked = TraitMgr->UnlockSlotWithDiamond(TargetBuilding->GetBuildingIndex(), SlotIndex);
	if (!bUnlocked)
	{
		// 확인~클릭 사이 잔액 변동 등 — 방어적 Toast
		if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
		{
			UIMgr->ShowNotification(NSLOCTEXT("Notification", "NotEnoughDiamond", "보석이 부족합니다"), 3.0f, ENotificationType::Failed);
		}
		return;
	}

	// 차감은 매니저 소관, 지출 피드백은 구매를 개시한 패널 응답 지점에서 (매니저를 UI 무관하게 유지)
	if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
	{
		if (UInGameLayerWidget* InGame = UIMgr->GetInGameLayer())
		{
			InGame->SpawnSpendPopup(EResourceType::Diamond, UBuildingTraitManagerSubsystem::GetDiamondCostForSlot(SlotIndex));
		}
	}
}

void UBuildingManagePanelWidget::HandleTraitCardClicked(FName TraitID)
{
	OpenTraitDetailForInventory(TraitID);
}

void UBuildingManagePanelWidget::EnsureTraitDetailPopup()
{
	// 정상 경로 = 트리 정적 배치(BindWidgetOptional). 아래 생성은 paste 소실 폴백.
	if (!TraitDetailPopup)
	{
		UClass* PopupClass = LoadClass<UTraitDetailPopupWidget>(nullptr,
			TEXT("/Game/CompanyGrowth/UI/Elements/Building/UIE_TraitDetailPopup.UIE_TraitDetailPopup_C"));
		if (!PopupClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("[BuildingManagePanelWidget] UIE_TraitDetailPopup 클래스 로드 실패"));
			return;
		}
		TraitDetailPopup = CreateWidget<UTraitDetailPopupWidget>(this, PopupClass);
		if (!TraitDetailPopup) return;
		UE_LOG(LogTemp, Warning, TEXT("[BuildingManagePanelWidget] TraitDetailPopup 정적 바인딩 소실 — 동적 생성 폴백"));
	}

	if (!TraitDetailPopup->GetParent() && TraitModalLayer)
	{
		// 폴백 부착 — 내비 사이 중앙 HBox 가 있으면 그 안에(끝에 붙어 순서는 어긋나지만 동작 우선), 없으면 레이어 중앙
		UHorizontalBox* CenterBox = TraitNavPrevButton ? Cast<UHorizontalBox>(TraitNavPrevButton->GetParent()) : nullptr;
		if (CenterBox)
		{
			CenterBox->AddChildToHorizontalBox(TraitDetailPopup);
		}
		else if (UOverlaySlot* PopupSlot = TraitModalLayer->AddChildToOverlay(TraitDetailPopup))
		{
			PopupSlot->SetHorizontalAlignment(HAlign_Center);
			PopupSlot->SetVerticalAlignment(VAlign_Center);
		}
	}

	// 단일캐스트 = 매 호출 재바인딩(교체) 가드 유지
	TraitDetailPopup->OnEquipClicked.BindUObject(this, &UBuildingManagePanelWidget::HandlePopupEquip);
	TraitDetailPopup->OnUnequipClicked.BindUObject(this, &UBuildingManagePanelWidget::HandlePopupUnequip);
	TraitDetailPopup->OnCloseClicked.BindUObject(this, &UBuildingManagePanelWidget::HandlePopupClose);
}

void UBuildingManagePanelWidget::OpenTraitDetailForInventory(FName TraitID)
{
	EnsureTraitDetailPopup();
	if (!TraitDetailPopup || !TargetBuilding) return;

	TraitDetailPopup->ConfigureForTrait(TraitID, TargetBuilding->GetBuildingIndex(), INDEX_NONE);

	// 타겟 힌트 ― HandlePopupEquip 의 타겟 결정 규칙과 동일 (명시 타겟 우선, 없으면 첫 빈 해금 슬롯)
	UBuildingTraitManagerSubsystem* TraitMgr = GetGameInstance()->GetSubsystem<UBuildingTraitManagerSubsystem>();
	const int32 BIdx = TargetBuilding->GetBuildingIndex();
	int32 HintTarget = PendingTargetSlotIndex;
	if (HintTarget == INDEX_NONE || !TraitMgr || !TraitMgr->IsSlotUnlocked(BIdx, HintTarget))
	{
		HintTarget = FindFirstEmptyUnlockedSlot();
	}
	if (TraitMgr && !TraitMgr->CanEquipTraitToBuilding(BIdx, TraitID))
	{
		// 업종 제한 특성 ― RestrictionText 가 사유를 표시하므로 타겟 힌트는 숨겨 메시지 상충 방지
		TraitDetailPopup->SetTargetSlotHint(FText::GetEmpty());
	}
	else if (HintTarget == INDEX_NONE)
	{
		TraitDetailPopup->SetTargetSlotHint(NSLOCTEXT("Trait", "TargetHintFull", "빈 슬롯이 없습니다 ― 닫고 슬롯을 눌러 교체할 수 있습니다"));
	}
	else
	{
		const TArray<FName> EquippedNow = TraitMgr ? TraitMgr->GetEquippedTraits(BIdx) : TArray<FName>();
		const bool bSwap = EquippedNow.IsValidIndex(HintTarget) && !EquippedNow[HintTarget].IsNone();
		TraitDetailPopup->SetTargetSlotHint(bSwap
			? FText::Format(NSLOCTEXT("Trait", "TargetHintSwap", "슬롯 {0} 특성과 교체됩니다"), FText::AsNumber(HintTarget + 1))
			: FText::Format(NSLOCTEXT("Trait", "TargetHint", "빈 슬롯 {0}에 장착됩니다"), FText::AsNumber(HintTarget + 1)));
	}

	CurrentDetailTraitID = TraitID;
	bDetailInventoryMode = true;
	UpdateTraitNavVisibility();
	TraitDetailPopup->SetVisibility(ESlateVisibility::Visible);
	if (TraitModalLayer) TraitModalLayer->SetVisibility(ESlateVisibility::Visible);
}

void UBuildingManagePanelWidget::OpenTraitDetailForSlot(int32 SlotIndex)
{
	EnsureTraitDetailPopup();
	if (!TraitDetailPopup || !TargetBuilding) return;
	UBuildingTraitManagerSubsystem* TraitMgr = GetGameInstance()->GetSubsystem<UBuildingTraitManagerSubsystem>();
	if (!TraitMgr) return;

	const TArray<FName> Equipped = TraitMgr->GetEquippedTraits(TargetBuilding->GetBuildingIndex());
	const FName TraitID = Equipped.IsValidIndex(SlotIndex) ? Equipped[SlotIndex] : NAME_None;
	if (TraitID.IsNone()) return;

	TraitDetailPopup->ConfigureForTrait(TraitID, TargetBuilding->GetBuildingIndex(), SlotIndex);
	TraitDetailPopup->SetTargetSlotHint(FText::GetEmpty());
	CurrentDetailTraitID = TraitID;
	bDetailInventoryMode = false;
	UpdateTraitNavVisibility();
	TraitDetailPopup->SetVisibility(ESlateVisibility::Visible);
	if (TraitModalLayer) TraitModalLayer->SetVisibility(ESlateVisibility::Visible);
}

void UBuildingManagePanelWidget::CloseTraitDetailPopup()
{
	if (TraitDetailPopup) TraitDetailPopup->SetVisibility(ESlateVisibility::Collapsed);
	if (TraitModalLayer) TraitModalLayer->SetVisibility(ESlateVisibility::Collapsed);
	CurrentDetailTraitID = NAME_None;
	bDetailInventoryMode = false;
}

void UBuildingManagePanelWidget::SetPendingTargetSlot(int32 SlotIndex)
{
	PendingTargetSlotIndex = SlotIndex;
	for (UTraitSlotWidget* SlotWidget : SpawnedTraitSlots)
	{
		if (SlotWidget) SlotWidget->SetSelected(SlotWidget->GetSlotIndex() == SlotIndex);
	}
}

int32 UBuildingManagePanelWidget::FindFirstEmptyUnlockedSlot() const
{
	if (!TargetBuilding) return INDEX_NONE;
	UBuildingTraitManagerSubsystem* TraitMgr = GetGameInstance()->GetSubsystem<UBuildingTraitManagerSubsystem>();
	if (!TraitMgr) return INDEX_NONE;

	const int32 BIdx = TargetBuilding->GetBuildingIndex();
	const TArray<FName> Equipped = TraitMgr->GetEquippedTraits(BIdx);
	const int32 MaxSlots = UBuildingTraitManagerSubsystem::GetMaxSlotCount();

	for (int32 i = 0; i < MaxSlots; ++i)
	{
		if (TraitMgr->IsSlotUnlocked(BIdx, i) && Equipped.IsValidIndex(i) && Equipped[i].IsNone())
		{
			return i;
		}
	}
	return INDEX_NONE;
}

void UBuildingManagePanelWidget::HandlePopupEquip(FName TraitID)
{
	if (!TargetBuilding) return;
	UBuildingTraitManagerSubsystem* TraitMgr = GetGameInstance()->GetSubsystem<UBuildingTraitManagerSubsystem>();
	if (!TraitMgr) return;

	const int32 BIdx = TargetBuilding->GetBuildingIndex();

	// 타겟 슬롯 결정: 명시 타겟(해금 상태) 우선, 없으면 첫 빈 해금 슬롯
	int32 Target = PendingTargetSlotIndex;
	if (Target == INDEX_NONE || !TraitMgr->IsSlotUnlocked(BIdx, Target))
	{
		Target = FindFirstEmptyUnlockedSlot();
	}

	if (Target == INDEX_NONE)
	{
		// 빈 해금 슬롯 없음 → Toast 로 사유 안내 (해금 0개 vs 꽉 참). silent return 금지.
		if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
		{
			const bool bNoUnlocked = (TraitMgr->GetUnlockedSlotCount(BIdx) == 0);
			UIMgr->ShowNotification(bNoUnlocked
				? NSLOCTEXT("Trait", "NoUnlockedSlot", "특성 슬롯을 먼저 해금하세요")
				: NSLOCTEXT("Trait", "NoEmptySlot", "빈 특성 슬롯이 없습니다 (해제 후 장착)"),
				3.0f, ENotificationType::Warning);
		}
		UE_LOG(LogTemp, Log, TEXT("[BuildingManagePanelWidget] 빈 해금 슬롯 없음 (Unlocked=%d)"), TraitMgr->GetUnlockedSlotCount(BIdx));
		return;
	}

	const TArray<FName> Equipped = TraitMgr->GetEquippedTraits(BIdx);
	const bool bOccupied = Equipped.IsValidIndex(Target) && !Equipped[Target].IsNone();

	const bool bOk = bOccupied
		? TraitMgr->ReplaceTrait(BIdx, Target, TraitID)
		: TraitMgr->EquipTrait(BIdx, Target, TraitID);

	if (bOk)
	{
		// 장착 성공 = 모달 닫힘 (B안) — 닫히면서 슬롯이 채워지는 것이 피드백. 타겟은 첫 빈 슬롯 복귀.
		CloseTraitDetailPopup();
		SetPendingTargetSlot(FindFirstEmptyUnlockedSlot());

		// 미션 가이드(M11 EquipFirstTrait) — 첫 특성 장착 = 완료
		if (UMissionManagerSubsystem* M = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>())
		{
			M->NotifyTraitEquipped();
		}
	}
	else
	{
		if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
		{
			UIMgr->ShowNotification(NSLOCTEXT("Trait", "EquipFailed", "장착할 수 없습니다"), 3.0f, ENotificationType::Failed);
		}
		UE_LOG(LogTemp, Log, TEXT("[BuildingManagePanelWidget] 장착 실패 (TraitID=%s, Slot=%d)"), *TraitID.ToString(), Target);
	}
}

void UBuildingManagePanelWidget::HandlePopupUnequip(int32 SlotIndex)
{
	if (!TargetBuilding) return;
	UBuildingTraitManagerSubsystem* TraitMgr = GetGameInstance()->GetSubsystem<UBuildingTraitManagerSubsystem>();
	if (!TraitMgr) return;

	TraitMgr->UnequipTrait(TargetBuilding->GetBuildingIndex(), SlotIndex);
	CloseTraitDetailPopup();
	SetPendingTargetSlot(INDEX_NONE);
}

void UBuildingManagePanelWidget::HandlePopupClose()
{
	CloseTraitDetailPopup();
	SetPendingTargetSlot(FindFirstEmptyUnlockedSlot());  // 닫으면 기본 선택(첫 빈 슬롯)으로 복귀
}

void UBuildingManagePanelWidget::UpdateTraitNavVisibility()
{
	// 내비 = 인벤 모드 + 카드 2장 이상일 때만 (해제 모달/외길에선 숨김)
	const bool bShow = bDetailInventoryMode && SpawnedTraitCards.Num() > 1;
	const ESlateVisibility V = bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
	if (TraitNavPrevButton) TraitNavPrevButton->SetVisibility(V);
	if (TraitNavNextButton) TraitNavNextButton->SetVisibility(V);
}

void UBuildingManagePanelWidget::NavigateTraitDetail(int32 Delta)
{
	if (!bDetailInventoryMode || SpawnedTraitCards.Num() < 2) return;

	int32 CurIdx = INDEX_NONE;
	for (int32 i = 0; i < SpawnedTraitCards.Num(); ++i)
	{
		if (SpawnedTraitCards[i] && SpawnedTraitCards[i]->GetTraitID() == CurrentDetailTraitID)
		{
			CurIdx = i;
			break;
		}
	}
	if (CurIdx == INDEX_NONE) CurIdx = 0;

	const int32 Num = SpawnedTraitCards.Num();
	const int32 NextIdx = ((CurIdx + Delta) % Num + Num) % Num;   // wrap-around 순환
	if (SpawnedTraitCards[NextIdx])
	{
		OpenTraitDetailForInventory(SpawnedTraitCards[NextIdx]->GetTraitID());
	}
}

void UBuildingManagePanelWidget::HandleTraitNavPrev() { NavigateTraitDetail(-1); }
void UBuildingManagePanelWidget::HandleTraitNavNext() { NavigateTraitDetail(+1); }

void UBuildingManagePanelWidget::OnTraitModalDimClicked() { HandlePopupClose(); }

void UBuildingManagePanelWidget::HandleTraitSlotChanged(int32 ChangedBuildingIndex, int32 SlotIndex, FName NewTraitID)
{
	if (!TargetBuilding || ChangedBuildingIndex != TargetBuilding->GetBuildingIndex()) return;

	// 장착/해제로 인벤토리도 변하므로 슬롯 + 카드 모두 갱신
	RebuildTraitSlots();
	RebuildTraitCardList();
	RefreshSetBonusBand();
}

void UBuildingManagePanelWidget::HandleTraitSlotUnlocked(int32 ChangedBuildingIndex, int32 SlotIndex)
{
	if (!TargetBuilding || ChangedBuildingIndex != TargetBuilding->GetBuildingIndex()) return;
	RebuildTraitSlots();
}

// ========== 스킨 탭 관련 함수들 ==========

void UBuildingManagePanelWidget::LoadAvailableSkins()
{
	if (!SkinCardContainer)
	{
		UE_LOG(LogTemp, Error, TEXT("[BuildingManagePanelWidget] SkinCardContainer not found!"));
		return;
	}

	// TableManagerSubsystem 가져오기
	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		UE_LOG(LogTemp, Error, TEXT("[BuildingManagePanelWidget] GameInstance not found!"));
		return;
	}

	UTableManagerSubsystem* TableManager = GameInstance->GetSubsystem<UTableManagerSubsystem>();
	if (!TableManager)
	{
		UE_LOG(LogTemp, Error, TEXT("[BuildingManagePanelWidget] TableManagerSubsystem not found!"));
		return;
	}

	// TableManager에서 BuildingSkinCard 위젯 클래스 가져오기
	TSubclassOf<UUserWidget> BuildingSkinCardClass = TableManager->GetWidgetClass(EWidgetType::BuildingSkinCard);
	if (!BuildingSkinCardClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[BuildingManagePanelWidget] BuildingSkinCard widget class not found!"));
		return;
	}

	// 기존 카드 제거 및 선택 상태 초기화
	SkinCardContainer->ClearChildren();
	CurrentlySelectedCard = nullptr;

	// 현재 건물에 적용된 스킨 ID 가져오기
	int32 CurrentAppliedSkinID = TargetBuilding ? TargetBuilding->GetAppliedSkinID() : 0;
	UE_LOG(LogTemp, Warning, TEXT("[BuildingManagePanelWidget] TargetBuilding: %s, CurrentAppliedSkinID: %d"),
		TargetBuilding ? *TargetBuilding->GetName() : TEXT("NULL"), CurrentAppliedSkinID);

	// 모든 희귀도의 스킨 로드
	TArray<FBuildingSkinData> AllSkins;

	// 모든 희귀도 레벨에서 스킨 가져오기
	AllSkins.Append(TableManager->GetBuildingSkinsByRarity(ELootBoxRarity::Common));
	AllSkins.Append(TableManager->GetBuildingSkinsByRarity(ELootBoxRarity::Unusual));
	AllSkins.Append(TableManager->GetBuildingSkinsByRarity(ELootBoxRarity::Rare));
	AllSkins.Append(TableManager->GetBuildingSkinsByRarity(ELootBoxRarity::Epic));
	AllSkins.Append(TableManager->GetBuildingSkinsByRarity(ELootBoxRarity::Legendary));
	AllSkins.Append(TableManager->GetBuildingSkinsByRarity(ELootBoxRarity::Mythic));

	if (AllSkins.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildingManagePanelWidget] No skins found in DataTable!"));
		return;
	}

	// 각 스킨마다 카드 생성
	for (const FBuildingSkinData& SkinData : AllSkins)
	{
		UBuildingSkinCardWidget* Card = CreateWidget<UBuildingSkinCardWidget>(
			this, BuildingSkinCardClass);

		if (Card)
		{
			// 스킨 잠금 해제 여부 확인
			bool bIsUnlocked = CheckIfSkinUnlocked(SkinData.SkinID);

			Card->SetSkinData(SkinData, bIsUnlocked);

			// 클릭 이벤트 바인딩
			Card->OnClicked().AddWeakLambda(this, [this, Card]()
			{
				OnSkinCardClicked(Card);
			});

			SkinCardContainer->AddChild(Card);

			// 현재 건물에 적용된 스킨이면 선택 상태로 표시
			if (SkinData.SkinID == CurrentAppliedSkinID)
			{
				Card->SetIsSelected(true);
				CurrentlySelectedCard = Card;
				UE_LOG(LogTemp, Log, TEXT("[BuildingManagePanelWidget] Auto-selected currently applied skin: %d"), SkinData.SkinID);
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[BuildingManagePanelWidget] Loaded %d skin cards"), AllSkins.Num());
}

void UBuildingManagePanelWidget::OnSkinCardClicked(UBuildingSkinCardWidget* ClickedCard)
{
	if (!ClickedCard)
	{
		return;
	}

	if (!ClickedCard->IsUnlocked())
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildingManagePanelWidget] Attempted to select locked skin"));
		if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
		{
			UIMgr->ShowNotification(NSLOCTEXT("Notification", "SkinLocked", "이 스킨은 잠금 해제되지 않았습니다"), 3.0f, ENotificationType::Failed);
		}
		return;
	}

	int32 SkinID = ClickedCard->GetSkinID();
	UE_LOG(LogTemp, Log, TEXT("[BuildingManagePanelWidget] Skin selected: ID %d"), SkinID);

	// 이전에 선택된 카드가 있으면 선택 해제
	if (CurrentlySelectedCard && CurrentlySelectedCard != ClickedCard)
	{
		CurrentlySelectedCard->ClearSelection();
		CurrentlySelectedCard->SetIsSelected(false);
		UE_LOG(LogTemp, Log, TEXT("[BuildingManagePanelWidget] Deselected previous card"));
	}

	// 새로운 카드 선택
	ClickedCard->SetIsSelected(true);
	CurrentlySelectedCard = ClickedCard;

	// 스킨 적용 (저장은 ApplySkin 내부에서 자동으로 처리됨)
	if (TargetBuilding)
	{
		TargetBuilding->ApplySkin(SkinID);
		UE_LOG(LogTemp, Log, TEXT("[BuildingManagePanelWidget] Applied skin %d to building"), SkinID);

		// 목표판 G8 EquipFirstSkin 조건 신호 (판정은 GoalBoard)
		if (UMissionManagerSubsystem* M = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>())
		{
			M->NotifySkinEquipped();
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildingManagePanelWidget] No target building set"));
	}
}

bool UBuildingManagePanelWidget::CheckIfSkinUnlocked(int32 SkinID) const
{
	// SaveLoadManager에서 보유 스킨 확인
	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildingManagePanelWidget] GameInstance not found when checking skin unlock"));
		return false;
	}

	USaveLoadManager* SaveLoadManager = GameInstance->GetSubsystem<USaveLoadManager>();
	if (!SaveLoadManager)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildingManagePanelWidget] SaveLoadManager not found when checking skin unlock"));
		return false;
	}

	// 현재 세이브 데이터 가져오기
	USaveGame_GameData* SaveData = SaveLoadManager->GetCurrentSaveData();
	if (!SaveData)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildingManagePanelWidget] SaveData not found when checking skin unlock"));
		return false;
	}

	// 플레이어가 이 스킨을 보유하고 있는지 확인
	const TArray<FBuildingSkinInstance>& OwnedSkins = SaveData->GameData.OwnedBuildingSkins;

	bool bIsOwned = OwnedSkins.ContainsByPredicate([SkinID](const FBuildingSkinInstance& Skin)
	{
		return Skin.SkinID == SkinID;
	});

	return bIsOwned;
}

// ========== 강화 탭 관련 함수들 ==========

void UBuildingManagePanelWidget::RebuildEnhancementSlots()
{
	if (!TargetBuilding || !EnhancementSlotContainer)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildingManagePanelWidget] RebuildEnhancementSlots: TargetBuilding 또는 Container 없음"));
		return;
	}

	// 모뉴먼트(키스톤)는 층수 강화(=영향권 성장)만 노출 — 일반 빌딩과 같은 UpgradeSlot/메커닉, 나머지 타입은 스킵
	const bool bKeystone = TargetBuilding->IsKeystoneMonument();

	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr)
	{
		UE_LOG(LogTemp, Error, TEXT("[BuildingManagePanelWidget] TableManagerSubsystem 없음"));
		return;
	}

	// UpgradeSlot 위젯 클래스 — EWidgetType::UpgradeSlot 으로 DT_Widget 에서 lookup (하드 참조 회피)
	TSubclassOf<UUserWidget> SlotClass = TableMgr->GetWidgetClass(EWidgetType::UpgradeSlot);
	if (!SlotClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[BuildingManagePanelWidget] UpgradeSlot 위젯 클래스 없음 — DT_Widget 에 EWidgetType::UpgradeSlot 행 추가 필요"));
		return;
	}

	const ECompanyType BuildingCompanyType = TargetBuilding->GetCompanyType();
	UE_LOG(LogTemp, Log, TEXT("[BuildingManagePanelWidget] Rebuilding enhancement slots - CompanyType: %s"),
		*CompanyTypeToString(BuildingCompanyType));

	// 기존 슬롯 전체 제거 + 캐시 초기화
	EnhancementSlotContainer->ClearChildren();
	DynamicSlots.Reset();
	LockedEnhancementTypes.Reset();

	// DT 기반 슬롯 목록 — CompanyType 에 해당하는 공통+전용 필터 + SortOrder 정렬
	const TArray<FBuildingEnhancementDefinition> Defs = TableMgr->GetEnhancementsForCompanyType(BuildingCompanyType);
	for (const FBuildingEnhancementDefinition& Def : Defs)
	{
		// 모뉴먼트는 두 슬롯만 노출 — 층수(BuildingFloor)=영향 범위, 영향력(KeystoneAuraPower)=버프 효과. 나머지 타입은 스킵
		if (bKeystone
			&& Def.EnhancementType != EBuildingEnhancementType::BuildingFloor
			&& Def.EnhancementType != EBuildingEnhancementType::KeystoneAuraPower)
		{
			continue;
		}

		// 역방향 — 영향력은 키스톤만. KeystoneAuraSubsystem 이 키스톤 액터만 순회하므로 일반 빌딩에선 소비처가 없다
		if (!bKeystone && Def.EnhancementType == EBuildingEnhancementType::KeystoneAuraPower)
		{
			continue;
		}

		UUpgradeSlot* NewSlot = CreateWidget<UUpgradeSlot>(this, SlotClass);
		if (!NewSlot) continue;

		// DT 정의 주입 (아이콘/이름/설명/단위/정수표시 여부)
		ApplySlotDefinition(NewSlot, Def);

		// 모뉴먼트 층수 슬롯은 같은 아이콘/메커닉을 쓰되 설명만 영향권 메시지로 덮어씀
		if (bKeystone && Def.EnhancementType == EBuildingEnhancementType::BuildingFloor)
		{
			NewSlot->SubDescription = FText::FromString(TEXT("영향 범위가 넓어집니다"));
		}
		// 영향력 슬롯은 버프 효과(%) 성장을 안내
		else if (bKeystone && Def.EnhancementType == EBuildingEnhancementType::KeystoneAuraPower)
		{
			NewSlot->SubDescription = FText::FromString(TEXT("영향권 버프가 강해집니다"));
		}

		FText LockConditionText;
		const bool bLocked = ResolveEnhancementLockState(Def.EnhancementType, LockConditionText);
		NewSlot->LockConditionText = LockConditionText;
		if (bLocked)
		{
			LockedEnhancementTypes.Add(Def.EnhancementType);
		}

		// 버튼 이벤트 바인딩 (BuildingFloor vs 홀드 슬롯 분기)
		BindSlotButton(NewSlot, Def.EnhancementType);

		EnhancementSlotContainer->AddChild(NewSlot);
		DynamicSlots.Add(Def.EnhancementType, NewSlot);

		// 초기 UI 갱신 (레벨/비용/Locked)
		UpdateEnhancementSlot(Def.EnhancementType, NewSlot, bLocked);
	}

	UE_LOG(LogTemp, Log, TEXT("[BuildingManagePanelWidget] Enhancement slots rebuilt: %d slots"), DynamicSlots.Num());
}

bool UBuildingManagePanelWidget::ResolveEnhancementLockState(
	EBuildingEnhancementType EnhancementType,
	FText& OutLockConditionText) const
{
	OutLockConditionText = FText::GetEmpty();

	const UGameInstance* GameInstance = GetGameInstance();
	const UMissionManagerSubsystem* MissionMgr = GameInstance
		? GameInstance->GetSubsystem<UMissionManagerSubsystem>()
		: nullptr;
	const bool bTutorialCompleted = MissionMgr && MissionMgr->IsTutorialCompleted();
	const bool bUnlockedByTier = IsEnhancementUnlockedByTier(EnhancementType);
	const bool bAuthorityAllowsPurchase = TargetBuilding
		&& TargetBuilding->IsEnhancementPurchaseAllowed(EnhancementType);

	const CGR::UI::EBuildingEnhancementLockReason LockReason = CGR::UI::ResolveEnhancementLockReason(
		EnhancementType,
		bTutorialCompleted,
		bUnlockedByTier,
		bAuthorityAllowsPurchase);

	if (LockReason == CGR::UI::EBuildingEnhancementLockReason::Tutorial)
	{
		OutLockConditionText = NSLOCTEXT("Building", "EnhUnlockAfterTutorial", "튜토리얼 완료 후 해금");
	}
	else if (LockReason == CGR::UI::EBuildingEnhancementLockReason::Tier)
	{
		const int32 UnlockTier = FindEnhancementUnlockTier(EnhancementType);
		OutLockConditionText = UnlockTier > 0
			? FText::Format(NSLOCTEXT("Building", "EnhUnlockAtTier", "{0}단계 달성 시 해금"), FText::AsNumber(UnlockTier))
			: FText::GetEmpty();
	}

	return LockReason != CGR::UI::EBuildingEnhancementLockReason::None;
}

bool UBuildingManagePanelWidget::IsEnhancementUnlockedByTier(EBuildingEnhancementType EnhancementType) const
{
	if (EnhancementType == EBuildingEnhancementType::BuildingFloor
		|| EnhancementType == EBuildingEnhancementType::KeystoneAuraPower)
	{
		return true;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GameInstance
		? GameInstance->GetSubsystem<UTableManagerSubsystem>()
		: nullptr;
	USaveLoadManager* SaveMgr = GameInstance
		? GameInstance->GetSubsystem<USaveLoadManager>()
		: nullptr;
	if (!TargetBuilding || !TableMgr || !SaveMgr)
	{
		return false;
	}

	const int32 BuildingTier = SaveMgr->GetBuildingTier(TargetBuilding->GetBuildingIndex());
	return TableMgr->GetUnlockedEnhancementsUpToTier(BuildingTier).Contains(EnhancementType);
}

int32 UBuildingManagePanelWidget::FindEnhancementUnlockTier(EBuildingEnhancementType EnhancementType) const
{
	UTableManagerSubsystem* TableMgr = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>()
		: nullptr;
	if (!TableMgr)
	{
		return 0;
	}

	for (int32 Tier = 1; Tier <= TierConstants::MAX_TIER; ++Tier)
	{
		bool bFound = false;
		const FTierUnlockData TierData = TableMgr->GetTierUnlockData(Tier, bFound);
		if (bFound && TierData.UnlockedEnhancements.Contains(EnhancementType))
		{
			return Tier;
		}
	}

	return 0;
}

bool UBuildingManagePanelWidget::IsEnhancementInteractionAllowed(EBuildingEnhancementType EnhancementType) const
{
	return TargetBuilding && TargetBuilding->IsEnhancementPurchaseAllowed(EnhancementType);
}

void UBuildingManagePanelWidget::RefreshEnhancementLocksIfTutorialStateChanged()
{
	const UMissionManagerSubsystem* MissionMgr = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>()
		: nullptr;
	if (!MissionMgr)
	{
		return;
	}

	const bool bTutorialCompleted = MissionMgr->IsTutorialCompleted();
	if (!CGR::UI::ShouldRefreshEnhancementLocksOnTutorialStateChange(
		bLastTutorialCompleted,
		bTutorialCompleted))
	{
		return;
	}

	bLastTutorialCompleted = bTutorialCompleted;
	RefreshAllEnhancementSlots();
}

void UBuildingManagePanelWidget::HandleMissionCompleted(
	FName,
	const FMissionTable&)
{
	RefreshEnhancementLocksIfTutorialStateChanged();
}

void UBuildingManagePanelWidget::ApplySlotDefinition(UUpgradeSlot* InSlot, const FBuildingEnhancementDefinition& Def)
{
	if (!InSlot) return;

	// DT 에서 정의된 표시용 값들을 UpgradeSlot 멤버에 주입 → NativePreConstruct 에서 UI 반영
	InSlot->Description = Def.DisplayName;
	InSlot->SubDescription = Def.SubDescription;
	InSlot->ValueUnit = Def.ValueUnit;
	InSlot->bIsInteger = Def.bIsInteger;
	InSlot->CostResourceType = Def.CostResourceType;

	// 아이콘: TSoftObjectPtr 포인터만 주입, 실제 로드는 UpgradeSlot 내부에서 처리 (비어있으면 건너뜀)
	if (!Def.Icon.IsNull())
	{
		InSlot->Icon = Def.Icon;
	}
}

void UBuildingManagePanelWidget::BindSlotButton(UUpgradeSlot* InSlot, EBuildingEnhancementType Type)
{
	if (!InSlot) return;
	UCostActionButtonWidget* Btn = InSlot->GetUpgradeButton();
	if (!Btn) return;

	// 안전하게 기존 바인딩 제거
	Btn->OnClicked().RemoveAll(this);
	Btn->OnPressed().RemoveAll(this);
	Btn->OnReleased().RemoveAll(this);

	if (Type == EBuildingEnhancementType::BuildingFloor)
	{
		// 층수는 클릭 1회만 (홀드 연속 업그레이드 X — 비주얼 변화가 크고 카메라 재포커싱 필요)
		Btn->OnClicked().AddUObject(this, &UBuildingManagePanelWidget::OnBuildingUpgradeButtonClicked);
	}
	else
	{
		// 나머지 슬롯: x1 = Pressed 1회 강화 + 홀드 타이머, x10 이상 = 탭당 벌크 1회 구매 (홀드 비활성)
		Btn->OnPressed().AddWeakLambda(this, [this, Type]()
		{
			if (!IsEnhancementInteractionAllowed(Type))
			{
				return;
			}
			if (BulkMode == EEnhanceBulkMode::x1)
			{
				OnEnhancementUpgradeClicked(Type);
				StartEnhancementHold(Type);
			}
			else
			{
				OnEnhancementBulkPurchase(Type);
			}
		});
		Btn->OnReleased().AddUObject(this, &UBuildingManagePanelWidget::StopEnhancementHold);
	}
}

void UBuildingManagePanelWidget::UpdateEnhancementSlot(EBuildingEnhancementType EnhancementType, UUpgradeSlot* UpgradeSlot, bool bForceLocked)
{
	if (!UpgradeSlot || !TargetBuilding)
	{
		return;
	}

	// 손상된 음수 레벨이 표시/비용 계산으로 전파되지 않게 액터 강화 경로와 같은 기준으로 정규화한다.
	const int32 RawCurrentLevel = TargetBuilding->GetEnhancementLevel(EnhancementType);
	const int32 CurrentLevel = FMath::Max(RawCurrentLevel, 0);

	// DT 정의 — MaxLevel(0=무제한) 과 표시 단위의 단일 진실
	int32 MaxLevel = 0;
	FBuildingEnhancementDefinition SlotDef;
	bool bHasSlotDef = false;
	if (UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>())
	{
		bHasSlotDef = TableMgr->GetEnhancementDefinition(EnhancementType, SlotDef);
		if (bHasSlotDef)
		{
			MaxLevel = SlotDef.MaxLevel;
		}
	}

	// 벌크 모드 여부 — BuildingFloor 는 마일스톤형이라 1차 토글 대상 제외, 잠금 슬롯도 단건 표시
	const bool bBulkMode = (BulkMode != EEnhanceBulkMode::x1)
		&& EnhancementType != EBuildingEnhancementType::BuildingFloor
		&& !bForceLocked;

	int32 BulkCount = 1;
	int64 BulkTotalCost = 0;
	int64 BulkAvailable = 0;
	if (bBulkMode)
	{
		BulkCount = ComputeBulkCount(EnhancementType, CurrentLevel, MaxLevel, BulkTotalCost, BulkAvailable);
	}

	// 배율 계산 — 벌크 모드 델타는 +N레벨 합산 효과 (Count=0 이면 1레벨 기준 유지)
	const int32 RequestedDelta = bBulkMode ? FMath::Max(BulkCount, 1) : 1;
	const int64 NextLevel64 = FMath::Clamp<int64>(
		static_cast<int64>(CurrentLevel) + static_cast<int64>(RequestedDelta),
		0LL,
		static_cast<int64>(MAX_int32));
	const int32 NextLevel = static_cast<int32>(NextLevel64);
	float CurrentMultiplier = TargetBuilding->GetEnhancementMultiplier(EnhancementType);
	float NextMultiplier = UBuildingEnhancementHelper::CalculateEffectMultiplier(EnhancementType, NextLevel);

	// 업그레이드 비용
	int64 Cost = TargetBuilding->GetUpgradeCost(EnhancementType);

	// 타입별로 UI 표시용 값 변환
	float DisplayCurrent = 0.0f;
	float DisplayNext = 0.0f;

	// 타입별 표시 값 변환 — 기준값(운영/오라행)이 없어 배율로 폴백하면 DT 단위(원/분)와 어긋나므로 아래에서 교정한다
	bool bMultiplierFallback = false;
	// 빌드업이 값 행에 인원을 쓰는지(= 직원을 받는 건물인지). 층수는 레벨 뱃지로 빠진다
	bool bFloorShowsCapacity = false;
	int32 CurrentFloorNumber = 1;

	switch (EnhancementType)
	{
	case EBuildingEnhancementType::BuildingFloor:
	{
		// 층수 자체보다 "몇 명 더 받나"가 구매 이유 — 값 행은 인원, 층수는 뱃지로.
		// 현재값을 세이브 경로(GetBuildingEmployeeCapacity)로 읽으면 강화 직후엔 지연 저장 전이라 왼쪽 숫자만 한 박자 늦는다 → 액터 층수 기준으로 계산
		UEmployeeManager* CapEmpMgr = GetGameInstance()->GetSubsystem<UEmployeeManager>();
		const int32 CapBuildingIndex = TargetBuilding->GetBuildingIndex();
		const int32 CurCap = CapEmpMgr ? CapEmpMgr->GetBuildingEmployeeCapacityAtFloors(CapBuildingIndex, CurrentLevel, /*bLogIfZero*/ false) : 0;
		const int32 NextCap = CapEmpMgr ? CapEmpMgr->GetBuildingEmployeeCapacityAtFloors(CapBuildingIndex, NextLevel, /*bLogIfZero*/ false) : 0;
		CurrentFloorNumber = static_cast<int32>(FMath::Min<int64>(
			static_cast<int64>(CurrentLevel) + 1LL,
			static_cast<int64>(MAX_int32)));
		const int32 NextFloorNumber = static_cast<int32>(FMath::Min<int64>(
			static_cast<int64>(NextLevel) + 1LL,
			static_cast<int64>(MAX_int32)));
		// 모뉴먼트는 직원을 안 받는다(FBuildingData::GetEmployeeCapacity 가 0) — 인원으로 쓰면 "0명"이 거짓이 된다
		bFloorShowsCapacity = (NextCap > 0);
		// 증축 0회가 1층 (Body_Module_Copies 가 레벨 정본)
		DisplayCurrent = static_cast<float>(bFloorShowsCapacity ? CurCap : CurrentFloorNumber);
		DisplayNext = static_cast<float>(bFloorShowsCapacity ? NextCap : NextFloorNumber);
		break;
	}

	case EBuildingEnhancementType::VaultCapacity:
		{
			// "금고 용량"이 약속하는 건 보관 금액이다 — 시간은 내부 곡선일 뿐 화면에 내지 않는다.
			// 레이트×시간을 여기서 다시 곱하면 CRM 특성·하한이 빠져 스탯 행과 다른 숫자가 나온다 → 정본에 위임.
			if (UProjectOperationManager* OpMgr = GetGameInstance()->GetSubsystem<UProjectOperationManager>())
			{
				const int32 VaultBuildingIndex = TargetBuilding->GetBuildingIndex();
				DisplayCurrent = OpMgr->CalculateWarehouseCapacityAtLevel(VaultBuildingIndex, CurrentLevel);
				DisplayNext = OpMgr->CalculateWarehouseCapacityAtLevel(VaultBuildingIndex, NextLevel);
			}
		}
		break;

	case EBuildingEnhancementType::EventResistance:
		// 악재 피해 잔존률 = 1/배율 (ProjectTraitEventHandler 의 PenaltyRetention 과 같은 식)
		DisplayCurrent = 100.0f / FMath::Max(CurrentMultiplier, KINDA_SMALL_NUMBER);
		DisplayNext = 100.0f / FMath::Max(NextMultiplier, KINDA_SMALL_NUMBER);
		break;

	case EBuildingEnhancementType::ViralBoost:
		// 호재 효과 = 배율 그대로 (ProjectTraitEventHandler 의 ViralMult)
		DisplayCurrent = CurrentMultiplier * 100.0f;
		DisplayNext = NextMultiplier * 100.0f;
		break;

	case EBuildingEnhancementType::ProjectLifespan:
		// 진행 중 운영의 TotalOperationTime 은 착수 시 1회 확정이라 강화해도 안 움직인다 —
		// 분 단위를 앵커로 쓰면 플레이어가 끝내 관측 못 할 숫자를 약속하게 된다. 배율이 덜 구체적이지만 정직하다.
		bMultiplierFallback = true;
		break;

	case EBuildingEnhancementType::KeystoneAuraPower:
		{
			// 오라 기본 효과에 KeystoneAuraPower DT의 EffectPerLevel 배수를 적용해 실제 버프와 표시를 일치시킨다.
			bool bAuraRowOk = false;
			FBuildingData AuraRow;
			if (UTableManagerSubsystem* AuraTableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>())
			{
				AuraRow = AuraTableMgr->GetBuildingData(TargetBuilding->GetBuildingID(), bAuraRowOk);
			}
			// 소비처의 스킵 조건(AuraTarget=None, Percent<=0)까지 같이 봐야 "안 걸리는 오라"를 숫자로 약속하지 않는다
			const float NextPercent = bAuraRowOk
				? AuraRow.KeystoneAura.BasePercent
					* UBuildingEnhancementHelper::CalculateEffectMultiplier(
						EBuildingEnhancementType::KeystoneAuraPower, NextLevel)
				: 0.0f;
			if (bAuraRowOk && AuraRow.KeystoneAura.bIsKeystone
				&& AuraRow.KeystoneAura.AuraTarget != EBuildingTraitTarget::None
				&& NextPercent > 0.0f)
			{
				DisplayCurrent = AuraRow.KeystoneAura.BasePercent
					* UBuildingEnhancementHelper::CalculateEffectMultiplier(
						EBuildingEnhancementType::KeystoneAuraPower, CurrentLevel);
				DisplayNext = NextPercent;
			}
			else
			{
				bMultiplierFallback = true;
			}
		}
		break;

	default:
		// 인재육성/브랜드파워/마케팅파워/프로젝트 산출량/프로젝트 등급 — 배율 그 자체가 읽히는 값.
		// 마케팅파워를 실수익(원)으로 앵커하면 운영 중일 때만 숫자가 서서 같은 슬롯이 원/% 를 오간다
		bMultiplierFallback = true;
		break;
	}

	// 배율 표시로 내려간 슬롯은 DT 단위(원/분)와 안 맞으므로 런타임에 보너스 % 로 교정.
	// "배"로 두면 1레벨 스텝(EffectPerLevel 0.0003~0.0005)이 소수 3자리 반올림에 통째로 먹혀 "1배 → 1배 (+0배)"가 된다
	if (bMultiplierFallback)
	{
		DisplayCurrent = (CurrentMultiplier - 1.0f) * 100.0f;
		DisplayNext = (NextMultiplier - 1.0f) * 100.0f;
		UpgradeSlot->ValueUnit = TEXT("%");
		UpgradeSlot->ValuePrefix = TEXT("+");
		UpgradeSlot->bIsInteger = false;
	}
	else if (bHasSlotDef)
	{
		// 운영이 다시 잡히면 폴백에서 빠져나오므로, 교정해 둔 % 표기를 DT 단위로 되돌려야 한다
		UpgradeSlot->ValueUnit = SlotDef.ValueUnit;
		UpgradeSlot->ValuePrefix.Reset();
		UpgradeSlot->bIsInteger = SlotDef.bIsInteger;
	}

	// 빌드업 뱃지 = 층수. 인원을 값 행에 쓸 때만 — 모뉴먼트는 값 행이 이미 층수라 뱃지까지 층수면 중복이다
	if (EnhancementType == EBuildingEnhancementType::BuildingFloor)
	{
		UpgradeSlot->SetLevelBadgeOverride(bFloorShowsCapacity
			? FText::Format(FText::FromString(TEXT("{0}층")), FText::AsNumber(CurrentFloorNumber))
			: FText::GetEmpty());
		if (!bFloorShowsCapacity)
		{
			UpgradeSlot->ValueUnit = TEXT("층");
		}
	}

	// 금고 슬롯만 금액을 만/억으로 축약 — 후반 자릿수가 슬롯을 터뜨리는 걸 막는다.
	// 폴백(%) 경로로 내려간 슬롯이 축약 플래그를 물려받지 않도록 매번 명시적으로 쓴다.
	UpgradeSlot->bAbbreviateValue = (EnhancementType == EBuildingEnhancementType::VaultCapacity) && !bMultiplierFallback;
	UpgradeSlot->SetSecondaryAnnotation(FText::GetEmpty());

	UpgradeSlot->UpdateInfo(CurrentLevel, DisplayCurrent, DisplayNext, Cost, bForceLocked, MaxLevel,
		bBulkMode, BulkCount, bBulkMode ? BulkTotalCost : Cost, BulkAvailable);

	UE_LOG(LogTemp, Log, TEXT("[BuildingManagePanelWidget] Updated slot: Type=%d, Lv=%d, Display=%.1f→%.1f, Cost=%lld, Locked=%d, Bulk=%d(x%d)"),
		(int32)EnhancementType, CurrentLevel, DisplayCurrent, DisplayNext, Cost, bForceLocked, bBulkMode, BulkCount);
}

void UBuildingManagePanelWidget::OnEnhancementUpgradeClicked(EBuildingEnhancementType EnhancementType)
{
	if (!TargetBuilding)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildingManagePanelWidget] OnEnhancementUpgradeClicked: TargetBuilding is null"));
		return;
	}
	if (!IsEnhancementInteractionAllowed(EnhancementType))
	{
		return;
	}

	// 업그레이드 시도
	bool bSuccess = TargetBuilding->UpgradeEnhancement(EnhancementType, 1);

	if (bSuccess)
	{
		UE_LOG(LogTemp, Log, TEXT("[BuildingManagePanelWidget] Enhancement upgraded: Type=%d"), (int32)EnhancementType);

		// 동적 슬롯 TMap 에서 해당 슬롯 찾아 UI 갱신 + 이펙트
		if (UUpgradeSlot* TargetSlot = DynamicSlots.FindRef(EnhancementType))
		{
			UpdateEnhancementSlot(EnhancementType, TargetSlot);
			TargetSlot->PlayUpgradeEffect();
		}

		// 기대 수익 갱신 — 모든 강화가 영향 줄 수 있음 (IncomeMult, StatBonus 등)
		if (UProjectOperationManager* OpMgr = GetGameInstance()->GetSubsystem<UProjectOperationManager>())
		{
			OpMgr->InvalidateStatBonusCache(TargetBuilding->GetBuildingIndex());
		}

	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildingManagePanelWidget] Failed to upgrade enhancement: Type=%d"), (int32)EnhancementType);
		if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
		{
			UIMgr->ShowNotification(NSLOCTEXT("Notification", "NotEnoughMoney", "자금이 부족합니다"), 3.0f, ENotificationType::Failed);
		}
	}
}

// ========== 강화 홀드 연속 강화 ==========
// BindEnhancementSlotButtons 는 BindSlotButton() 로 통합됨 (슬롯 생성 시 자동 바인딩)

void UBuildingManagePanelWidget::StartEnhancementHold(EBuildingEnhancementType EnhancementType)
{
	if (!IsEnhancementInteractionAllowed(EnhancementType))
	{
		return;
	}

	CurrentHoldEnhancementType = EnhancementType;

	// 홀드 중 재구매 펀치 억제 (10Hz 연타 노이즈 차단) — Stop 에서 해제 쌍
	if (UUpgradeSlot* HeldSlot = DynamicSlots.FindRef(EnhancementType))
	{
		HeldSlot->SetPunchSuppressed(true);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			EnhancementHoldTimerHandle,
			this,
			&UBuildingManagePanelWidget::OnEnhancementHoldTick,
			HoldRepeatInterval,
			true,
			HoldInitialDelay
		);
	}
}

void UBuildingManagePanelWidget::StopEnhancementHold()
{
	if (UUpgradeSlot* HeldSlot = DynamicSlots.FindRef(CurrentHoldEnhancementType))
	{
		HeldSlot->SetPunchSuppressed(false);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EnhancementHoldTimerHandle);
	}
}

void UBuildingManagePanelWidget::OnEnhancementHoldTick()
{
	if (!IsEnhancementInteractionAllowed(CurrentHoldEnhancementType))
	{
		StopEnhancementHold();
		return;
	}

	bool bSuccess = TargetBuilding->UpgradeEnhancement(CurrentHoldEnhancementType, 1);
	if (bSuccess)
	{
		// 동적 슬롯 TMap 에서 찾아 갱신 — 모든 홀드 슬롯이 여기를 통함
		if (UUpgradeSlot* TargetSlot = DynamicSlots.FindRef(CurrentHoldEnhancementType))
		{
			UpdateEnhancementSlot(CurrentHoldEnhancementType, TargetSlot);
			TargetSlot->PlayUpgradeEffect();
		}

		// 기대 수익 갱신 — 연속 강화 중에도 수익 배율 변화 즉시 반영
		if (UProjectOperationManager* OpMgr = GetGameInstance()->GetSubsystem<UProjectOperationManager>())
		{
			OpMgr->InvalidateStatBonusCache(TargetBuilding->GetBuildingIndex());
		}
	}
	else
	{
		// 자금 부족 등으로 실패 시 홀드 중단
		StopEnhancementHold();
	}
}

// ========== 강화 배율 / 벌크 구매 ==========

void UBuildingManagePanelWidget::ApplyBulkMode(EEnhanceBulkMode NewMode)
{
	if (BulkMode == NewMode)
	{
		// 동일 모드 재적용 — 선택기 체크 상태만 정합 유지
		if (BulkModeSelector)
		{
			BulkModeSelector->SetMode(BulkMode);
		}
		return;
	}
	BulkMode = NewMode;

	// 모드 전환 중 홀드가 살아있으면 정리 (x1 → 벌크 전환 직후 유령 홀드 방지)
	StopEnhancementHold();

	if (BulkModeSelector)
	{
		BulkModeSelector->SetMode(BulkMode);
	}
	RefreshAllEnhancementSlots();
}

void UBuildingManagePanelWidget::RefreshAllEnhancementSlots()
{
	for (const TPair<EBuildingEnhancementType, UUpgradeSlot*>& Pair : DynamicSlots)
	{
		if (Pair.Value)
		{
			FText LockConditionText;
			ResolveEnhancementLockState(Pair.Key, LockConditionText);
			if (!Pair.Value->LockConditionText.EqualTo(LockConditionText))
			{
				// 잠금 사유 텍스트는 슬롯 생성 시 위젯 트리에 반영되므로 사유가 바뀔 때만 재생성한다.
				RebuildEnhancementSlots();
				return;
			}
		}
	}

	LockedEnhancementTypes.Reset();
	for (const TPair<EBuildingEnhancementType, UUpgradeSlot*>& Pair : DynamicSlots)
	{
		if (Pair.Value)
		{
			FText LockConditionText;
			const bool bLocked = ResolveEnhancementLockState(Pair.Key, LockConditionText);
			Pair.Value->LockConditionText = LockConditionText;
			if (bLocked)
			{
				LockedEnhancementTypes.Add(Pair.Key);
			}
			UpdateEnhancementSlot(Pair.Key, Pair.Value, LockedEnhancementTypes.Contains(Pair.Key));
		}
	}
}

int32 UBuildingManagePanelWidget::ComputeBulkCount(EBuildingEnhancementType EnhancementType, int32 CurrentLevel, int32 MaxLevel,
	int64& OutTotalCost, int64& OutAvailable) const
{
	OutTotalCost = 0;
	OutAvailable = 0;
	const int32 NormalizedCurrentLevel = FMath::Max(CurrentLevel, 0);

	// 비용 자원 타입은 DT 단일 진실 — 하드코딩 금지
	const EResourceType CostType = UBuildingEnhancementHelper::GetCostResourceType(EnhancementType);
	if (UResourceItemManager* ResMgr = GetGameInstance()->GetSubsystem<UResourceItemManager>())
	{
		OutAvailable = ResMgr->GetResourceAmount(CostType);
	}

	int32 Count = 1;
	switch (BulkMode)
	{
	case EEnhanceBulkMode::x10: Count = 10; break;
	case EEnhanceBulkMode::x50: Count = 50; break;
	default: break;
	}

	// 요청 수량·액터 하드 가드(100)·int32 레벨 headroom을 모두 int64에서 제한한다.
	const int64 RequestedCount = FMath::Clamp<int64>(static_cast<int64>(Count), 0LL, 100LL);
	const int64 LevelHeadroom = static_cast<int64>(MAX_int32) - static_cast<int64>(NormalizedCurrentLevel);
	int64 AllowedCount = FMath::Clamp<int64>(LevelHeadroom, 0LL, RequestedCount);

	// MaxLevel(0=무제한)이어도 위 기술적 headroom 제한은 유지한다.
	if (MaxLevel > 0)
	{
		const int64 RemainingToMaxLevel = static_cast<int64>(MaxLevel) - static_cast<int64>(NormalizedCurrentLevel);
		AllowedCount = FMath::Clamp<int64>(RemainingToMaxLevel, 0LL, AllowedCount);
	}
	Count = static_cast<int32>(AllowedCount);

	// Count=0(최대 모드 빈털터리)이어도 부족액 표기는 1레벨 비용 기준.
	// ⚠ 칸수는 청구(액터)와 같은 출처여야 표시=청구가 유지된다 — TargetBuilding 없으면 1칸으로 폴백.
	OutTotalCost = UBuildingEnhancementHelper::CalculateBulkUpgradeCost(
		EnhancementType, NormalizedCurrentLevel, FMath::Max(Count, 1),
		TargetBuilding ? TargetBuilding->GetFootprintCells() : 1);
	return Count;
}

void UBuildingManagePanelWidget::OnEnhancementBulkPurchase(EBuildingEnhancementType EnhancementType)
{
	if (!TargetBuilding)
	{
		return;
	}
	if (!IsEnhancementInteractionAllowed(EnhancementType))
	{
		return;
	}

	const int32 CurrentLevel = TargetBuilding->GetEnhancementLevel(EnhancementType);

	int32 MaxLevel = 0;
	if (UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>())
	{
		FBuildingEnhancementDefinition Def;
		if (TableMgr->GetEnhancementDefinition(EnhancementType, Def))
		{
			MaxLevel = Def.MaxLevel;
		}
	}

	int64 TotalCost = 0;
	int64 Available = 0;
	const int32 Count = ComputeBulkCount(EnhancementType, CurrentLevel, MaxLevel, TotalCost, Available);

	// 1차 방어는 UpgradeBtn 의 부족 입력 삼킴 — 여기는 최대 모드 0레벨 등 잔여 경로의 방어적 안내
	if (Count <= 0 || Available < TotalCost)
	{
		if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
		{
			UIMgr->ShowNotification(NSLOCTEXT("Notification", "NotEnoughMoney", "자금이 부족합니다"), 3.0f, ENotificationType::Failed);
		}
		return;
	}

	const bool bSuccess = TargetBuilding->UpgradeEnhancement(EnhancementType, Count);
	if (!bSuccess)
	{
		if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
		{
			UIMgr->ShowNotification(NSLOCTEXT("Notification", "NotEnoughMoney", "자금이 부족합니다"), 3.0f, ENotificationType::Failed);
		}
		return;
	}

	// 벌크 1회 구매 = 이펙트 1회
	if (UUpgradeSlot* TargetSlot = DynamicSlots.FindRef(EnhancementType))
	{
		TargetSlot->PlayUpgradeEffect();
	}

	// 자금이 크게 움직였으므로 전 슬롯의 벌크 카운트/부족 캡션 재계산
	RefreshAllEnhancementSlots();

	// 기대 수익 갱신 — 단건 경로와 동일
	if (UProjectOperationManager* OpMgr = GetGameInstance()->GetSubsystem<UProjectOperationManager>())
	{
		OpMgr->InvalidateStatBonusCache(TargetBuilding->GetBuildingIndex());
	}

}

// ========== 오피스 입장 ==========

UWidget* UBuildingManagePanelWidget::GetEnterOfficeButtonWidget() const
{
	return EnterOfficeButton;
}

UWidget* UBuildingManagePanelWidget::GetTraitTabButtonWidget() const
{
	return TraitTab ? TraitTab->GetButton() : nullptr;
}

UWidget* UBuildingManagePanelWidget::GetTraitEquipButtonIfOpen() const
{
	if (!TraitDetailPopup || !TraitDetailPopup->IsVisible())
	{
		return nullptr;
	}

	UWidget* Equip = TraitDetailPopup->GetEquipButtonWidget();
	if (!Equip)
	{
		return nullptr;
	}

	// 점유 슬롯에서 연 팝업은 [해제] 모드라 [장착]이 Collapsed — null 을 돌려줘야 호출부의 슬롯 폴백이 작동한다
	const ESlateVisibility Vis = Equip->GetVisibility();
	if (Vis == ESlateVisibility::Collapsed || Vis == ESlateVisibility::Hidden || !Equip->GetIsEnabled())
	{
		return nullptr;
	}

	return Equip;
}

UWidget* UBuildingManagePanelWidget::GetFirstTraitSlotWidget() const
{
	if (!TraitSlotRow)
	{
		return nullptr;
	}

	// 밴드에 스페이서/라벨이 끼어도 첫 "슬롯"을 집는다 — RebuildTraitSlots 와 같은 계약
	for (int32 i = 0; i < TraitSlotRow->GetChildrenCount(); ++i)
	{
		if (UTraitSlotWidget* SlotWidget = Cast<UTraitSlotWidget>(TraitSlotRow->GetChildAt(i)))
		{
			return SlotWidget;
		}
	}
	return nullptr;
}

UWidget* UBuildingManagePanelWidget::GetFirstOwnedTraitCardWidget() const
{
	return (SpawnedTraitCards.Num() > 0) ? SpawnedTraitCards[0].Get() : nullptr;
}

UWidget* UBuildingManagePanelWidget::GetOwnedTraitCardWidgetById(FName InTraitID) const
{
	UTraitCardSlotWidget* FirstCard = nullptr;
	for (const TObjectPtr<UTraitCardSlotWidget>& Card : SpawnedTraitCards)
	{
		if (!Card) continue;
		if (!FirstCard) { FirstCard = Card.Get(); }
		if (Card->GetTraitID() == InTraitID) { return Card.Get(); }
	}
	// 못 찾으면 첫 카드 — 링이 사라지는 것보단 낫다(ID 미지정 NAME_None 도 여기로 떨어진다)
	return FirstCard;
}

UWidget* UBuildingManagePanelWidget::GetSkinTabButtonWidget() const
{
	return SkinTab ? SkinTab->GetButton() : nullptr;
}

UWidget* UBuildingManagePanelWidget::GetFirstSkinCardWidget() const
{
	if (!SkinCardContainer) return nullptr;
	for (int32 i = 0; i < SkinCardContainer->GetChildrenCount(); ++i)
	{
		if (UBuildingSkinCardWidget* Card = Cast<UBuildingSkinCardWidget>(SkinCardContainer->GetChildAt(i)))
		{
			return Card;
		}
	}
	return nullptr;
}

UWidget* UBuildingManagePanelWidget::GetSkinCardWidgetById(int32 InSkinID) const
{
	if (!SkinCardContainer) return nullptr;
	UBuildingSkinCardWidget* FirstCard = nullptr;
	for (int32 i = 0; i < SkinCardContainer->GetChildrenCount(); ++i)
	{
		UBuildingSkinCardWidget* Card = Cast<UBuildingSkinCardWidget>(SkinCardContainer->GetChildAt(i));
		if (!Card) continue;
		if (!FirstCard) { FirstCard = Card; }
		if (Card->GetSkinID() == InSkinID) { return Card; }
	}
	// 못 찾으면 첫 카드 — 링이 사라지는 것보단 낫다(ID 미지정 0 도 여기로 떨어진다)
	return FirstCard;
}

UWidget* UBuildingManagePanelWidget::GetEnhancementTabButtonWidget() const
{
	return EnhancementTab ? EnhancementTab->GetButton() : nullptr;
}

UWidget* UBuildingManagePanelWidget::GetBuildingFloorUpgradeButtonWidget() const
{
	// 빌드업 슬롯은 [강화] 탭 활성 시에만 생성·표시되므로, 강화탭이 아닐 땐 링 생략(null).
	if (!IsEnhancementTabActive()) return nullptr;
	if (UUpgradeSlot* FloorSlot = DynamicSlots.FindRef(EBuildingEnhancementType::BuildingFloor))
	{
		return FloorSlot->GetUpgradeButton();
	}
	return nullptr;
}

void UBuildingManagePanelWidget::OnEnterOfficeButtonClicked()
{
	UE_LOG(LogTemp, Log, TEXT("[BuildingManagePanelWidget] Enter Office button clicked"));

	if (!TargetBuilding)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildingManagePanelWidget] TargetBuilding is null"));
		return;
	}

	// GameInstance에 오피스 모드 설정 (Normal - 직원 선택 후 메뉴 표시)
	UCGGameInstance* GameInstance = Cast<UCGGameInstance>(GetGameInstance());
	if (!GameInstance)
	{
		UE_LOG(LogTemp, Error, TEXT("[BuildingManagePanelWidget] GameInstance is null"));
		return;
	}

	// 현재 관리 중인 건물 저장
	GameInstance->SetCurrentManagedBuilding(TargetBuilding);
	GameInstance->SetOfficeMode(EOfficeMode::Normal);

	UE_LOG(LogTemp, Warning, TEXT("[BuildingManagePanelWidget] *** SAVING TO GAMEINSTANCE ***"));
	UE_LOG(LogTemp, Warning, TEXT("[BuildingManagePanelWidget] TargetBuilding: %s"), *TargetBuilding->GetName());
	UE_LOG(LogTemp, Warning, TEXT("[BuildingManagePanelWidget] TargetBuilding BuildingIndex: %d"), TargetBuilding->GetBuildingIndex());
	UE_LOG(LogTemp, Warning, TEXT("[BuildingManagePanelWidget] TargetBuilding Address: %p"), TargetBuilding);

	// 레벨 전환 전 현재 데이터 저장 (Pending 카드 등 메모리 데이터 보존)
	if (USaveLoadManager* SaveMgr = GameInstance->GetSubsystem<USaveLoadManager>())
	{
		if (!SaveMgr->SaveGameData())
		{
			UE_LOG(LogTemp, Error, TEXT("[BuildingManagePanelWidget] Failed to save game data before level transition!"));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[BuildingManagePanelWidget] Game data saved before level transition"));
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[BuildingManagePanelWidget] Transitioning to Office scene (Normal mode)"));

	// 오피스 씬으로 전환 (TransitionToLevel 사용해서 MapType 설정)
	GameInstance->TransitionToLevel(TEXT("OfficeMap"));
}

// ========== 좌측 패널 건물 정보 ==========

void UBuildingManagePanelWidget::HandleLevelChipClicked()
{
	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	if (!GI) { return; }
	UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	UUIManagerSubsystem* UIMgr = GI->GetSubsystem<UUIManagerSubsystem>();
	if (!TableMgr || !UIMgr || !UIMgr->GetUIBase()) { return; }

	TSubclassOf<UUserWidget> Cls = TableMgr->GetWidgetClass(EWidgetType::TierRoadmap);
	if (!Cls)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildingManagePanel] TierRoadmap 위젯 미등록(DT_WidgetClass)"));
		return;
	}

	// 관리 패널을 닫지 않고 위에 적층 — 로드맵을 닫으면 이 패널이 그대로 남아야 한다
	UCommonActivatableWidget* W = UIMgr->GetUIBase()->PushPromptClass(Cls.Get());
	if (UTierRoadmapWidget* Roadmap = Cast<UTierRoadmapWidget>(W))
	{
		Roadmap->ConfigureForBuilding(GI->GetCurrentManagedBuildingIndex());
	}
}

void UBuildingManagePanelWidget::PopulateBuildingStats()
{
	if (!BuildingStatContainer || !TargetBuilding) return;

	BuildingStatContainer->ClearChildren();
	BuildingStatRows.Empty();

	// TableManager에서 StatRow 위젯 클래스 가져오기
	UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	if (!TableMgr) return;

	TSubclassOf<UUserWidget> StatRowClass = TableMgr->GetWidgetClass(EWidgetType::StatRow);
	if (!StatRowClass) return;

	// StatRow 생성 헬퍼
	auto CreateStatRow = [&](const FString& Name, const FString& Value) -> UStatRowWidget*
	{
		UStatRowWidget* Row = CreateWidget<UStatRowWidget>(this, StatRowClass);
		if (Row)
		{
			Row->SetStatInfo(Name, Value);
			BuildingStatContainer->AddChild(Row);
			BuildingStatRows.Add(Row);
		}
		return Row;
	};

	// 1. 업종
	ECompanyType CT = TargetBuilding->GetCompanyType();
	CreateStatRow(TEXT("업종"), CompanyTypeToString(CT));

	// 2. 건물 레벨 → 레벨 배지 (레벨업 델리게이트도 같은 함수를 호출한다)
	RefreshLevelBadge();

	// 3. 층수
	int32 Floor = TargetBuilding->GetEnhancementLevel(EBuildingEnhancementType::BuildingFloor) + 1;
	CreateStatRow(TEXT("층수"), FString::Printf(TEXT("%d층"), Floor));

	// 3. 직원 수 — 정원 게이트와 같은 술어(로스터=벤치 포함). 착석만 세면 게이트와 화면이 갈라진다
	int32 EmpCur = 0;
	if (UEmployeeManager* EmpMgr = GetGameInstance()->GetSubsystem<UEmployeeManager>())
	{
		EmpCur = EmpMgr->GetEmployeeCountInBuilding(TargetBuilding->GetBuildingIndex());
	}
	CreateStatRow(TEXT("직원"), FString::Printf(TEXT("%d명"), EmpCur));

	// 4. 업종별 수익/주문 표시
	if (IsProjectType(CT))
	{
		// 프로젝트 업종: 패시브 수익 (초당) — 표시 순수익(감쇠O/진동X), 축약 규약 준수
		FString RevenueStr = UGlobalUtilFunctions::AbbreviateNumber(0).ToString() + TEXT("/초");
		if (UProjectOperationManager* OpMgr = GetGameInstance()->GetSubsystem<UProjectOperationManager>())
		{
			const float Rate = OpMgr->GetBuildingDisplayNetPerSec(TargetBuilding->GetBuildingIndex());
			RevenueStr = UGlobalUtilFunctions::AbbreviateNumber(static_cast<int64>(Rate)).ToString() + TEXT("/초");
		}
		CreateStatRow(TEXT("수익"), RevenueStr);
	}
	else if (IsManufacturingType(CT))
	{
		// 제조업: 남은 주문 수량
		FString OrderStr = TEXT("없음");
		if (UProductionOrderManager* ProdMgr = GetGameInstance()->GetSubsystem<UProductionOrderManager>())
		{
			int32 Remaining = ProdMgr->GetTotalRemainingQuantityByBuilding(TargetBuilding->GetBuildingIndex());
			if (Remaining > 0)
			{
				OrderStr = FString::Printf(TEXT("%d개"), Remaining);
			}
		}
		CreateStatRow(TEXT("주문"), OrderStr);
	}

	// 5. 금고 용량 — 보관 가능 금액. 기준레이트가 티어 기반이라 운영 유무로 흔들리지 않는다.
	// 레벨은 강화 슬롯과 같은 액터 기준으로 읽는다 — 세이브 경로는 강화 직후 지연 저장 전이라 슬롯과 숫자가 갈린다.
	if (UProjectOperationManager* VaultOpMgr = GetGameInstance()->GetSubsystem<UProjectOperationManager>())
	{
		const int32 VaultLevel = TargetBuilding->GetEnhancementLevel(EBuildingEnhancementType::VaultCapacity);
		const int64 VaultCap = static_cast<int64>(
			VaultOpMgr->CalculateWarehouseCapacityAtLevel(TargetBuilding->GetBuildingIndex(), VaultLevel));
		CreateStatRow(TEXT("금고"), FString::Printf(TEXT("%s원"), *UGlobalUtilFunctions::AbbreviateNumber(VaultCap).ToString()));
	}

	UE_LOG(LogTemp, Log, TEXT("[BuildingManagePanelWidget] PopulateBuildingStats: %d rows created"), BuildingStatRows.Num());
}

void UBuildingManagePanelWidget::RefreshLevelBadge()
{
	int32 BldgTier = 1;
	int32 BIdx = INDEX_NONE;
	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	if (GI)
	{
		BIdx = GI->GetCurrentManagedBuildingIndex();
		if (USaveLoadManager* SM = GI->GetSubsystem<USaveLoadManager>())
		{
			BldgTier = SM->GetBuildingTier(BIdx);
		}
	}
	if (LevelText)
	{
		LevelText->SetText(FText::AsNumber(BldgTier));
	}
}

void UBuildingManagePanelWidget::UpdateProjectStatus()
{
	if (!ProjectStatusText || !TargetBuilding) return;

	ECompanyType CT = TargetBuilding->GetCompanyType();

	if (CT == ECompanyType::None)
	{
		ProjectStatusText->SetText(FText::FromString(TEXT("업종 미설정")));
		return;
	}

	// 활성 운영 데이터 확인
	UProjectOperationManager* OpMgr = GetGameInstance()->GetSubsystem<UProjectOperationManager>();
	FOperationData* OpData = OpMgr ? OpMgr->GetOperationByBuildingID(TargetBuilding->GetBuildingIndex()) : nullptr;

	if (IsProjectType(CT))
	{
		// 프로젝트 업종: 운영 데이터 확인
		if (OpData && OpData->RemainingTime > 0.f)
		{
			FString StatusStr = FString::Printf(TEXT("%s\n%s 남음"),
				*OpData->ProjectName, *OpData->GetRemainingTimeString());
			ProjectStatusText->SetText(FText::FromString(StatusStr));
		}
		else if (OpMgr && OpMgr->HasPendingReport(TargetBuilding->GetBuildingIndex()))
		{
			ProjectStatusText->SetText(FText::FromString(TEXT("프로젝트 완료")));
		}
		else
		{
			ProjectStatusText->SetText(FText::FromString(TEXT("프로젝트 대기")));
		}
	}
	else if (IsManufacturingType(CT))
	{
		// 제조업: 주문서 확인
		if (UProductionOrderManager* ProdMgr = GetGameInstance()->GetSubsystem<UProductionOrderManager>())
		{
			TArray<FProductionOrder> Orders = ProdMgr->GetOrdersByBuilding(TargetBuilding->GetBuildingIndex());
			if (Orders.Num() > 0)
			{
				const FProductionOrder& FirstOrder = Orders[0];
				int32 TotalRemaining = ProdMgr->GetTotalRemainingQuantityByBuilding(TargetBuilding->GetBuildingIndex());
				FString StatusStr = FString::Printf(TEXT("%s\n%d개 남음"),
					*FirstOrder.ProductName, TotalRemaining);
				ProjectStatusText->SetText(FText::FromString(StatusStr));
			}
			else
			{
				ProjectStatusText->SetText(FText::FromString(TEXT("생산 대기")));
			}
		}
		else
		{
			ProjectStatusText->SetText(FText::FromString(TEXT("생산 대기")));
		}
	}
	else
	{
		ProjectStatusText->SetText(FText::FromString(TEXT("업종 미설정")));
	}
}

void UBuildingManagePanelWidget::HandleOperationUpdated(int32 BuildingID, const FOperationData& Data)
{
	if (!TargetBuilding || TargetBuilding->GetBuildingIndex() != BuildingID) return;
	UpdateProjectStatus();
}

void UBuildingManagePanelWidget::HandleOperationCompleted(int32 BuildingID, const FOperationData& Data)
{
	if (!TargetBuilding || TargetBuilding->GetBuildingIndex() != BuildingID) return;
	UpdateProjectStatus();
	PopulateBuildingStats();
}

void UBuildingManagePanelWidget::HandleExpectedRevenueChanged(int32 BuildingID, float NewRate)
{
	// 내 TargetBuilding 의 이벤트만 처리 — 다른 빌딩 이벤트는 무시
	if (!TargetBuilding || TargetBuilding->GetBuildingIndex() != BuildingID) return;

	// "수익" 라벨은 PopulateBuildingStats 내에서 생성되므로 전체 재빌드
	PopulateBuildingStats();
}

void UBuildingManagePanelWidget::OnExpectedRevenuePollTick()
{
	if (!TargetBuilding) return;

	// 1분 주기로 내 빌딩의 수익을 재계산 요청 — OpMgr 가 델리게이트 브로드캐스트 → HandleExpectedRevenueChanged 에서 UI 갱신
	if (UProjectOperationManager* OpMgr = GetGameInstance()->GetSubsystem<UProjectOperationManager>())
	{
		OpMgr->RefreshExpectedRevenue(TargetBuilding->GetBuildingIndex());
	}
}

// ========== 스킨 탭 내부 외관/조명 토글 ==========

void UBuildingManagePanelWidget::SetupSkinSubTabGroup()
{
	SkinSubTabGroup = NewObject<UCommonButtonGroupBase>(this);
	if (!SkinSubTabGroup) return;

	SkinSubTabGroup->SetSelectionRequired(true);

	// SetIsInteractableWhenSelected(true) — selected 상태에서도 같은 버튼 재클릭 받게
	auto Setup = [this](UButtonWidget* Btn, void (UBuildingManagePanelWidget::*Handler)())
	{
		if (!Btn) return;
		Btn->SetIsSelectable(true);
		Btn->SetIsInteractableWhenSelected(true);
		SkinSubTabGroup->AddWidget(Btn);
		Btn->OnPressed().AddUObject(this, Handler);
	};
	Setup(ExteriorBtn, &UBuildingManagePanelWidget::HandleExteriorBtnClicked);
	Setup(LightingBtn, &UBuildingManagePanelWidget::HandleLightingBtnClicked);

	SkinSubTabGroup->SelectButtonAtIndex(0);  // 외관 기본
	if (SkinSwitcher)
	{
		SkinSwitcher->SetActiveWidgetIndex(0);
	}
}

void UBuildingManagePanelWidget::HandleExteriorBtnClicked()
{
	if (SkinSwitcher)
	{
		SkinSwitcher->SetActiveWidgetIndex(0);
	}
}

void UBuildingManagePanelWidget::HandleLightingBtnClicked()
{
	if (SkinSwitcher)
	{
		// WBP에서 조명 ScrollBox 슬롯이 인덱스 1
		SkinSwitcher->SetActiveWidgetIndex(1);
	}
}

// ========== 조명 카드 로드/클릭 ==========

void UBuildingManagePanelWidget::LoadAvailableLights()
{
	if (!SkinLightCardContainer)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildingManagePanelWidget] SkinLightCardContainer not found!"));
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance) return;

	UTableManagerSubsystem* TableManager = GameInstance->GetSubsystem<UTableManagerSubsystem>();
	if (!TableManager) return;

	TSubclassOf<UUserWidget> LightCardClass = TableManager->GetWidgetClass(EWidgetType::BuildingLightCard);
	if (!LightCardClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[BuildingManagePanelWidget] BuildingLightCard widget class not found in DT_Widget!"));
		return;
	}

	SkinLightCardContainer->ClearChildren();
	CurrentlySelectedLightCard = nullptr;

	const int32 CurrentAppliedLightID = TargetBuilding ? TargetBuilding->GetAppliedLightID() : 0;
	TArray<FBuildingLightData> AllLights = TableManager->GetAllBuildingLights();

	for (const FBuildingLightData& LightData : AllLights)
	{
		UBuildingLightCardWidget* Card = CreateWidget<UBuildingLightCardWidget>(this, LightCardClass);
		if (!Card) continue;

		const bool bIsUnlocked = CheckIfLightUnlocked(LightData.LightID);
		Card->SetLightData(LightData, bIsUnlocked);

		Card->OnClicked().AddWeakLambda(this, [this, Card]()
		{
			OnLightCardClicked(Card);
		});

		SkinLightCardContainer->AddChild(Card);

		if (LightData.LightID == CurrentAppliedLightID)
		{
			Card->SetIsSelected(true);
			CurrentlySelectedLightCard = Card;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[BuildingManagePanelWidget] Loaded %d light cards"), AllLights.Num());
}

void UBuildingManagePanelWidget::OnLightCardClicked(UBuildingLightCardWidget* ClickedCard)
{
	if (!ClickedCard) return;

	if (!ClickedCard->IsUnlocked())
	{
		if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
		{
			UIMgr->ShowNotification(NSLOCTEXT("Notification", "LightLocked", "이 조명은 잠금 해제되지 않았습니다"), 3.0f, ENotificationType::Failed);
		}
		return;
	}

	const int32 LightID = ClickedCard->GetLightID();

	// 이전 선택 해제
	if (CurrentlySelectedLightCard && CurrentlySelectedLightCard != ClickedCard)
	{
		CurrentlySelectedLightCard->ClearSelection();
		CurrentlySelectedLightCard->SetIsSelected(false);
	}

	ClickedCard->SetIsSelected(true);
	CurrentlySelectedLightCard = ClickedCard;

	if (TargetBuilding)
	{
		TargetBuilding->ApplyLight(LightID);
	}
}

bool UBuildingManagePanelWidget::CheckIfLightUnlocked(int32 LightID) const
{
	// 모든 조명 임시 해금 — 추후 BM/뽑기 시스템 연동 시 인벤토리 보유 여부 체크로 교체
	return true;
}
