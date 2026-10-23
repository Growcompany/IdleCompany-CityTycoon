// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Panel/HQManagePanelWidget.h"
#include "UI/Element/Common/ModalFrameWidget.h"
#include "UI/Element/HQ/HQConditionCardWidget.h"
#include "UI/Element/HQ/HQKpiCellWidget.h"
#include "UI/Element/HQ/HQIndustryCardWidget.h"
#include "UI/Element/HQ/HQLevelStepWidget.h"
#include "UI/Element/HQ/HQRewardSlotWidget.h"
#include "UI/Element/Buttons/MenuTileWidget.h"
#include "UI/Element/Common/RewardChipUtils.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/WrapBox.h"
#include "Engine/Texture2D.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Data/FactoryUpgradeData.h"
#include "Data/HQLevelUpReward.h"
#include "Data/WorldMapTypes.h"
#include "Table/BuildableCardTable.h"
#include "Table/BuildingData.h"
#include "Table/MenuUnlockData.h"
#include "Manager/PanelIntroSubsystem.h"
#include "TimerManager.h"
#include "UI/Panel/InGameLayerWidget.h"
#include "UI/Element/Common/ResourceWidget.h"
#include "Enum/ResourceType.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/UIManagerSubsystem.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/EntityManager.h"
#include "Manager/EmployeeManager.h"
#include "Manager/OfficeStageProgressManager.h"
#include "Manager/ProjectOperationManager.h"
#include "UI/UIBase.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Buttons/CostActionButtonWidget.h"
#include "UI/Element/Buttons/IconWithButtonWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "UI/Element/Building/BuildingSlotHorizonWidget.h"
#include "UI/Panel/BuildingManagePanelWidget.h"
#include "UI/Panel/HQLevelUpCelebrationWidget.h"
#include "CommonAnimatedSwitcher.h"
#include "CommonTextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/UniformGridSlot.h"
#include "Components/UniformGridPanel.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "Components/Button.h"
#include "Groups/CommonButtonGroupBase.h"
#include "Entity/Building/BuildingBaseActor.h"
#include "Player/MainMapPlayerController.h"
#include "Player/PlayerCamera.h"
#include "Enum/WidgetType.h"
#include "Enum/CompanyType.h"
#include "Data/GameSaveData.h"
#include "Table/CompanyInfoTable.h"
#include "Global/GlobalUtilFunctions.h"
#include "Manager/SoundManagerSubsystem.h"
#include "Manager/SettingsManagerSubsystem.h"
#include "UI/UISoundTags.h"

namespace
{
	// 미달은 '경고'가 아니라 '진행 중'이라 액센트 블루, 달성만 그린
	const FLinearColor ProgressPendingColor(0.047f, 0.328f, 0.745f);
	const FLinearColor ProgressMetColor(0.263f, 0.788f, 0.369f);

	// 남색 레벨 띠 위 상태 글자 (목업 대응표 §5)
	const FLinearColor HeroStatusInk(FColor(0xDC, 0xE6, 0xF1));
	const FLinearColor HeroReadyInk(FColor(0x9B, 0xE2, 0x7D));
	const FLinearColor HeroPercentInk = FLinearColor::White;
	const FLinearColor HeroMaxInk(FColor(0xFF, 0xD6, 0x6B));

	// 한 줄에 안 들어가는 해금 문구는 괄호 설명 앞에서 줄을 나눈다 — 괄호 안에서 끊기지 않게
	FText KeepHQUnlockDetailTogether(const FText& Text, const UCommonTextBlock& Block)
	{
		const FString Source = Text.ToString();
		const int32 DetailStart = Source.Find(TEXT(" ("));
		const float WrapWidth = Block.GetWrapTextAt();
		if (DetailStart == INDEX_NONE || WrapWidth <= 0.f || !FSlateApplication::IsInitialized()) return Text;
		const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		if (Measure->Measure(Source, Block.GetFont()).X <= WrapWidth) return Text;
		return FText::FromString(Source.Left(DetailStart) + TEXT("\n") + Source.Mid(DetailStart + 1));
	}
}

UWidget* UHQManagePanelWidget::GetGrowthGuideTargetWidget(bool* bOutConditionsUnmet) const
{
	if (bOutConditionsUnmet)
	{
		*bOutConditionsUnmet = IsGrowthTabActive() && !bCanLevelUpNow;
		if (*bOutConditionsUnmet) { return ConditionGrid; }
	}
	return IsGrowthTabActive() ? static_cast<UWidget*>(LevelUpButton) : static_cast<UWidget*>(HQTab);
}

UWidget* UHQManagePanelWidget::GetCloseButtonWidget() const
{
	return ModalFrame ? ModalFrame->GetCloseButton() : nullptr;
}

void UHQManagePanelWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	// 최초 진입 코치마크 — 등장 모션이 끝난 뒤 딤을 덮는다
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(
			PanelIntroTimer, this, &UHQManagePanelWidget::TryPlayPanelIntro, 0.5f, false);
	}

	// 자식 모달(레벨업 연출)을 push 하면 컨테이너가 이 패널을 deactivate 해 Normal 로 내려가는데, 재활성 시 아무도 되돌리지 않는다(BuildingManagePanel 과 같은 짝)
	if (AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr))
	{
		if (PC->GetCurrentInputMode() != EInputMode::UI)
		{
			PC->GoToUIMode();
		}
	}

	TabButtonGroup = NewObject<UCommonButtonGroupBase>(this);
	TabButtonGroup->SetSelectionRequired(true);

	// RailTab은 CommonButtonBase 파생 — 그룹 직접 등록. 등록 순서 = ContentSwitcher 인덱스
	auto RegisterTab = [this](UIconWithButtonWidget* Tab)
	{
		if (!Tab) return;
		TabButtonGroup->AddWidget(Tab);
		Tab->SetIsSelectable(true);
		// 전환음은 OnTabSelectionChanged 가 낸다 — 버튼 자체 클릭음을 끊어 두 발을 막는다
		Tab->SetPlayClickSound(false);
	};
	RegisterTab(HQTab);
	RegisterTab(OperationsTab);
	RegisterTab(BuildingListTab);

	TabButtonGroup->OnSelectedButtonBaseChanged.AddDynamic(this, &UHQManagePanelWidget::OnTabSelectionChanged);

	if (LevelUpButton)
	{
		LevelUpButton->OnClicked().AddUObject(this, &UHQManagePanelWidget::OnLevelUpButtonClicked);
		LevelUpButton->SetPunchSuppressed(true);
		LevelUpButton->SetButtonText(NSLOCTEXT("HQ", "LevelUpButtonLabel", "레벨업"));
	}

	if (BackgroundBtn)
	{
		BackgroundBtn->OnClicked.AddDynamic(this, &UHQManagePanelWidget::OnCloseButtonClicked);
	}

	if (UCloseButtonWidget* PanelCloseButton = ModalFrame ? ModalFrame->GetCloseButton() : nullptr)
	{
		PanelCloseButton->OnCloseClicked.AddDynamic(this, &UHQManagePanelWidget::OnCloseButtonClicked);
	}

	// 기본 탭 리셋은 무전이로 — 재사용된 패널은 스위처가 지난 탭에 남아 있어 역방향 페이드로 방금 닫은 화면이 다시 보인다
	UCommonAnimatedSwitcher* AnimatedSwitcher = Cast<UCommonAnimatedSwitcher>(ContentSwitcher);
	if (AnimatedSwitcher)
	{
		AnimatedSwitcher->SetDisableTransitionAnimation(true);
	}
	TabButtonGroup->SelectButtonAtIndex(GrowthTabIndex, false);
	CurrentTabIndex = GrowthTabIndex;
	if (ContentSwitcher) ContentSwitcher->SetActiveWidgetIndex(GrowthTabIndex);
	if (AnimatedSwitcher)
	{
		AnimatedSwitcher->SetDisableTransitionAnimation(false);
	}

	// 이 패널은 매 push마다 CreateWidget — 그룹 초기화도 NativeOnActivated에서 (파일 고유 idiom)
	SetupSortButtonGroup();

	// ===== claim 라이브 갱신 — 조건 변동원 구독 (해제는 NativeOnDeactivated에서 짝) =====

	if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
	{
		SaveMgr->OnHQLevelUp.AddDynamic(this, &UHQManagePanelWidget::HandleHQLevelUp);
	}

	// 돈 변동 — 패널 열린 채 돈이 차오를 때 조건/CTA 실시간 갱신 (claim 어포던스의 핵심)
	if (UResourceItemManager* ResMgr = GetGameInstance()->GetSubsystem<UResourceItemManager>())
	{
		ResMgr->OnResourceChanged.AddUObject(this, &UHQManagePanelWidget::HandleResourceChanged);
	}

	if (UEmployeeManager* EmpMgr = GetGameInstance()->GetSubsystem<UEmployeeManager>())
	{
		EmpMgr->OnEmployeeHireCompleted.AddUObject(this, &UHQManagePanelWidget::HandleEmployeeHireCompleted);
	}

	// 스테이지 완료 — World 서브시스템이라 MainMap에선 null (OfficeMap에서만 유효)
	if (UWorld* World = GetWorld())
	{
		if (UOfficeStageProgressManager* StageMgr = World->GetSubsystem<UOfficeStageProgressManager>())
		{
			StageMgr->OnStageComplete.AddDynamic(this, &UHQManagePanelWidget::HandleStageComplete);
		}
	}

	// 장식 VFX 텍스처 주입 — DT_UIVFXTexture 단일 진실 (WBP엔 빈 Image만 배치)
	if (UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>())
	{
		TableMgr->ApplyUIVFXTexture(TEXT("CTAShine"), CTAShineImage);
		TableMgr->ApplyUIVFXTexture(TEXT("CTAShine"), LevelBadgeGloss);
	}
	VFXTimer = 0.f;
	for (UImage* ImageWidget : {LevelBadgeGloss.Get(), CTAShineImage})
	{
		if (ImageWidget)
		{
			ImageWidget->SetRenderOpacity(0.f);
			ImageWidget->SetRenderTranslation(FVector2D::ZeroVector);
		}
	}

	UpdateHQInfo();
}

void UHQManagePanelWidget::TryPlayPanelIntro()
{
	// 타이머 대기 중 창이 닫혔거나, 튜토리얼이 이미 같은 설명을 띄웠다(덮어쓰면 그쪽 완료 기록이 끊긴다)
	if (!IsActivated() || PanelIntroOverlay.IsValid())
	{
		return;
	}
	UPanelIntroSubsystem* IntroMgr = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UPanelIntroSubsystem>() : nullptr;
	if (!IntroMgr)
	{
		return;
	}
	// 함수 지역 static 이라 유니티 빌드 셰도잉이 없다
	static const FName IntroKey(TEXT("ManufacturingUnlock"));
	if (!IsGrowthTabActive() || !IntroMgr->ShouldPlay(IntroKey)) { return; }
	PanelIntroOverlay = IntroMgr->PlayIntro(this, IntroKey, /*bIgnoreSeen=*/false);
	if (UPanelIntroOverlayWidget* Overlay = PanelIntroOverlay.Get())
	{
		Overlay->OnIntroFinished.AddUObject(this, &UHQManagePanelWidget::HandlePanelIntroFinished);
	}
	else if (GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(PanelIntroTimer, this,
			&UHQManagePanelWidget::TryPlayPanelIntro, 0.5f, false);
	}
}

void UHQManagePanelWidget::HandlePanelIntroFinished()
{
	PanelIntroOverlay.Reset();
}

bool UHQManagePanelWidget::PlayPanelIntro()
{
	if (PanelIntroOverlay.IsValid())
	{
		return true;
	}
	UPanelIntroSubsystem* IntroMgr = (IsActivated() && GetGameInstance())
		? GetGameInstance()->GetSubsystem<UPanelIntroSubsystem>() : nullptr;
	if (!IntroMgr)
	{
		return false;
	}
	static const FName IntroKey(TEXT("HQManage"));
	PanelIntroOverlay = IntroMgr->PlayIntro(this, IntroKey, /*bIgnoreSeen=*/true);
	if (UPanelIntroOverlayWidget* Overlay = PanelIntroOverlay.Get())
	{
		Overlay->OnIntroFinished.AddUObject(this, &UHQManagePanelWidget::HandlePanelIntroFinished);
		return true;
	}
	return false;
}

void UHQManagePanelWidget::NativeOnDeactivated()
{
	if (TabButtonGroup)
	{
		TabButtonGroup->OnSelectedButtonBaseChanged.RemoveAll(this);
	}

	if (SortLevelBtn)    SortLevelBtn->OnPressed().RemoveAll(this);
	if (SortIncomeBtn)   SortIncomeBtn->OnPressed().RemoveAll(this);
	if (SortEmployeeBtn) SortEmployeeBtn->OnPressed().RemoveAll(this);

	if (LevelUpButton)
	{
		LevelUpButton->OnClicked().RemoveAll(this);
	}
	bCTAShineActive = false;
	VFXTimer = 0.f;
	for (UImage* ImageWidget : {LevelBadgeGloss.Get(), CTAShineImage})
	{
		if (ImageWidget)
		{
			ImageWidget->SetRenderOpacity(0.f);
			ImageWidget->SetRenderTranslation(FVector2D::ZeroVector);
		}
	}

	if (BackgroundBtn)
	{
		BackgroundBtn->OnClicked.RemoveDynamic(this, &UHQManagePanelWidget::OnCloseButtonClicked);
	}

	if (UCloseButtonWidget* PanelCloseButton = ModalFrame ? ModalFrame->GetCloseButton() : nullptr)
	{
		PanelCloseButton->OnCloseClicked.RemoveAll(this);
	}

	if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
	{
		SaveMgr->OnHQLevelUp.RemoveDynamic(this, &UHQManagePanelWidget::HandleHQLevelUp);
	}

	if (UResourceItemManager* ResMgr = GetGameInstance()->GetSubsystem<UResourceItemManager>())
	{
		ResMgr->OnResourceChanged.RemoveAll(this);
	}

	if (UEmployeeManager* EmpMgr = GetGameInstance()->GetSubsystem<UEmployeeManager>())
	{
		EmpMgr->OnEmployeeHireCompleted.RemoveAll(this);
	}

	if (UWorld* World = GetWorld())
	{
		if (UOfficeStageProgressManager* StageMgr = World->GetSubsystem<UOfficeStageProgressManager>())
		{
			StageMgr->OnStageComplete.RemoveDynamic(this, &UHQManagePanelWidget::HandleStageComplete);
		}
	}

	// 인계 중이 아닐 때만 원복 — 인계 중이면 UI 모드도 가림 고스트도 방금 연 관리 패널 소유다
	if (!bHandingOffToManagePanel)
	{
		AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(GetWorld()->GetFirstPlayerController());
		if (PC)
		{
			// [이동]에서 후임 관리 패널 몫으로 포커스를 등록하므로(OnBuildingMoveClicked), 인계가 아닌 닫힘이면 그 등록을 여기서 끈다
			if (APlayerCamera* Camera = Cast<APlayerCamera>(PC->GetPawn()))
			{
				Camera->ClearFocusTarget();
			}

			// UI 모드 복원 — BuildOpenWidget에서 GoToUIMode()로 전환했으므로 닫힐 때 되돌림
			if (PC->GetCurrentInputMode() == EInputMode::UI)
			{
				PC->GoToNormalMode();
			}
		}
	}
	bHandingOffToManagePanel = false;

	Super::NativeOnDeactivated();

	// 인트로는 뷰포트 오버레이라 스택이 안 거둬간다 — RemoveFromParent → NativeDestruct 가 복구 신호를 쏜다
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(PanelIntroTimer);
	}
	if (UPanelIntroOverlayWidget* Intro = PanelIntroOverlay.Get())
	{
		Intro->RemoveFromParent();
	}
	PanelIntroOverlay.Reset();
}

void UHQManagePanelWidget::OnTabSelectionChanged(UCommonButtonBase* SelectedButton, int32 ButtonIndex)
{
	if (USoundManagerSubsystem* SM = GetGameInstance()->GetSubsystem<USoundManagerSubsystem>())
	{
		SM->PlayUISound(CGUISoundTags::TabSwitch);
	}

	CurrentTabIndex = ButtonIndex;
	if (ButtonIndex == GrowthTabIndex && IsActivated() && GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(PanelIntroTimer, this,
			&UHQManagePanelWidget::TryPlayPanelIntro, 0.5f, false);
	}
	if (ContentSwitcher)
	{
		ContentSwitcher->SetActiveWidgetIndex(ButtonIndex);
	}
	if (ButtonIndex != GrowthTabIndex)
	{
		for (UImage* ImageWidget : {LevelBadgeGloss.Get(), CTAShineImage})
		{
			if (ImageWidget) ImageWidget->SetRenderOpacity(0.f);
		}
	}

	if (ButtonIndex == OperationsTabIndex)
	{
		// 업종 수익은 열린 동안 구독하지 않는다 — 탭에 들어올 때 최신값으로 다시 집계
		int64 CurrentMoney = 0;
		if (UResourceItemManager* ResMgr = GetGameInstance()->GetSubsystem<UResourceItemManager>())
		{
			CurrentMoney = ResMgr->GetResourceAmount(EResourceType::Money);
		}
		UpdateOperations(CurrentMoney);
	}
	else if (ButtonIndex == BuildingsTabIndex)
	{
		PopulateBuildingList();
	}
	UpdateGrowthTabAlert();
}

void UHQManagePanelWidget::UpdateHQInfo()
{
	USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>();
	UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	if (!SaveMgr || !TableMgr) return;

	USaveGame_GameData* SaveData = SaveMgr->GetCurrentSaveData();
	if (!SaveData) return;

	CachedHQLevel = SaveMgr->GetHQLevel();
	const int32 NextLevel = CachedHQLevel + 1;
	bool bNextSuccess = false;
	const FHQLevelData NextData = TableMgr->GetHQLevelData(NextLevel, bNextSuccess);

	int32 TotalEmployees = 0;
	for (const auto& Pair : SaveData->GameData.OfficeDataMap)
	{
		TotalEmployees += Pair.Value.EmployeeList.Num();
	}

	int64 CurrentMoney = 0;
	int64 CurrentMarketCap = 0;
	if (UResourceItemManager* ResMgr = GetGameInstance()->GetSubsystem<UResourceItemManager>())
	{
		CurrentMoney = ResMgr->GetResourceAmount(EResourceType::Money);
		CurrentMarketCap = ResMgr->GetResourceAmount(EResourceType::MarketCap);
	}

	// 경량 경로(RefreshAffordability)용 스냅샷 캐시
	bNextLevelAvailable = bNextSuccess;
	CachedNextData = NextData;
	CachedBuildingCount = SaveData->GameData.Buildings.Num();
	CachedTierBuildingCount = (bNextSuccess && NextData.RequiredTier > 0) ? SaveMgr->CountBuildingsAtTier(NextData.RequiredTier) : 0;
	CachedTotalEmployees = TotalEmployees;
	CachedMarketCap = CurrentMarketCap;

	if (CurLevelText)
	{
		CurLevelText->SetText(FText::AsNumber(CachedHQLevel));
	}
	if (HeroTitleText)
	{
		HeroTitleText->SetText(bNextSuccess
			? NSLOCTEXT("HQ", "HeroNextLevel", "다음 레벨")
			: NSLOCTEXT("HQ", "HeroMaxLevel", "최고 레벨"));
	}
	if (HeroSubText)
	{
		HeroSubText->SetVisibility(bNextSuccess ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		if (bNextSuccess)
		{
			HeroSubText->SetText(FText::Format(NSLOCTEXT("HQ", "HeroUntilLevel", "Lv.{0}까지"), FText::AsNumber(NextLevel)));
		}
	}

	if (GrowthBody) GrowthBody->SetVisibility(bNextSuccess ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (MaxBody) MaxBody->SetVisibility(bNextSuccess ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);

	if (bNextSuccess)
	{
		UpdateGrowthBody(NextLevel);
	}
	else
	{
		PopulateLevelSteps();
	}

	// 운영 탭은 들어갈 때 OnTabSelectionChanged 가 다시 집계한다 — 안 보이는 동안 카드를 만들면 창 여는 프레임만 무거워진다
	if (CurrentTabIndex == OperationsTabIndex)
	{
		UpdateOperations(CurrentMoney);
	}
	UpdateOverallProgress(CurrentMoney);
	UpdateLevelUpButtonState();
}

void UHQManagePanelWidget::UpdateGrowthBody(int32 NextLevel)
{
	int64 CurrentMoney = 0;
	if (UResourceItemManager* ResMgr = GetGameInstance()->GetSubsystem<UResourceItemManager>())
	{
		CurrentMoney = ResMgr->GetResourceAmount(EResourceType::Money);
	}

	const FText BuildingUnit = NSLOCTEXT("HQ", "BuildingUnit", "개");
	UpdateCurrencyCondition(Row_Cond_Money, NSLOCTEXT("HQ", "MoneyConditionName", "보유 자금"), CurrentMoney, CachedNextData.MoneyCost);
	UpdateCountCondition(Row_Cond_Buildings, NSLOCTEXT("HQ", "BuildingConditionName", "보유 빌딩"),
		CachedBuildingCount, CachedNextData.RequiredBuildingCount, BuildingUnit);
	UpdateCountCondition(Row_Cond_Tier,
		FText::Format(NSLOCTEXT("HQ", "TierCondition", "{0}단계 이상 빌딩"), FText::AsNumber(CachedNextData.RequiredTier)),
		CachedTierBuildingCount, CachedNextData.RequiredTier > 0 ? CachedNextData.RequiredTierBuildingCount : 0, BuildingUnit);
	if (Row_Cond_Tier) Row_Cond_Tier->SetTierBadge(CachedNextData.RequiredTier);
	UpdateCountCondition(Row_Cond_Employees, NSLOCTEXT("HQ", "EmployeeConditionName", "전체 직원"),
		CachedTotalEmployees, CachedNextData.RequiredEmployeeCount, NSLOCTEXT("HQ", "EmployeeUnit", "명"));
	UpdateCurrencyCondition(Row_Cond_MarketCap, NSLOCTEXT("HQ", "MarketCapConditionName", "시가총액"), CachedMarketCap, CachedNextData.RequiredMarketCap);

	// 쓰지 않는 조건 칸을 회수하고 보이는 카드만 2열로 채운다
	int32 ConditionIndex = 0;
	constexpr int32 ConditionColumns = 2;
	for (UHQConditionCardWidget* Card : {Row_Cond_Money, Row_Cond_Buildings, Row_Cond_Tier, Row_Cond_Employees, Row_Cond_MarketCap})
	{
		if (!Card) continue;
		UUniformGridSlot* GridSlot = Cast<UUniformGridSlot>(Card->Slot);
		if (!GridSlot) continue;
		const bool bVisible = Card->GetVisibility() != ESlateVisibility::Collapsed;
		GridSlot->SetRow(bVisible ? ConditionIndex / ConditionColumns : 0);
		GridSlot->SetColumn(bVisible ? ConditionIndex % ConditionColumns : 0);
		if (bVisible) ++ConditionIndex;
	}

	if (RewardTitleText)
	{
		RewardTitleText->SetText(FText::Format(NSLOCTEXT("HQ", "NextLevelRewardTitle", "Lv.{0} 레벨업 보상"), FText::AsNumber(NextLevel)));
	}
	PopulateRewardSlots(NextLevel);
	if (UnlockText)
	{
		const FText Reward = BuildLevelRewardText(NextLevel);
		UnlockText->SetText(KeepHQUnlockDetailTogether(Reward, *UnlockText));
		UnlockText->SetVisibility(Reward.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
	if (LevelUpButton)
	{
		LevelUpButton->SetCost(CachedNextData.MoneyCost, EResourceType::Money, true);
	}
}

void UHQManagePanelWidget::PopulateLevelSteps()
{
	if (!LevelStepBox) return;
	LevelStepBox->ClearChildren();

	UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	if (!TableMgr) return;
	TSubclassOf<UUserWidget> StepClass = TableMgr->GetWidgetClass(EWidgetType::HQLevelStep);
	if (!StepClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[HQManage] DT_WidgetClass row missing for HQLevelStep"));
		return;
	}

	// 40레벨은 한 줄에 못 담는다 — 기능·업종이 열린 레벨(DT 문구가 있는 레벨)과 마지막 레벨만, 많으면 뒤쪽 다섯
	TArray<int32> Levels;
	for (int32 Level = 2; Level <= CachedHQLevel; ++Level)
	{
		bool bFound = false;
		const FHQLevelData LevelData = TableMgr->GetHQLevelData(Level, bFound);
		if (bFound && (!LevelData.UnlockDescription.IsEmpty() || Level == CachedHQLevel)) Levels.Add(Level);
	}
	constexpr int32 MaxSteps = 5;
	if (Levels.Num() > MaxSteps) Levels.RemoveAt(0, Levels.Num() - MaxSteps);

	for (int32 Index = 0; Index < Levels.Num(); ++Index)
	{
		const int32 Level = Levels[Index];
		UHQLevelStepWidget* Step = CreateWidget<UHQLevelStepWidget>(this, StepClass);
		if (!Step) continue;
		Step->SetStep(Level, BuildLevelRewardText(Level), Level == CachedHQLevel);
		if (UHorizontalBoxSlot* StepSlot = LevelStepBox->AddChildToHorizontalBox(Step))
		{
			StepSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			StepSlot->SetPadding(FMargin(Index == 0 ? 0.f : 12.f, 0.f, Index == Levels.Num() - 1 ? 0.f : 12.f, 0.f));
		}
	}
}

void UHQManagePanelWidget::UpdateOperations(int64 CurrentMoney)
{
	const FText NoUnit = FText::GetEmpty();
	if (Kpi_Money) Kpi_Money->SetValue(UGlobalUtilFunctions::AbbreviateNumber(CurrentMoney), NoUnit);
	if (Kpi_MarketCap) Kpi_MarketCap->SetValue(UGlobalUtilFunctions::AbbreviateNumber(CachedMarketCap), NoUnit);
	if (Kpi_Buildings) Kpi_Buildings->SetValue(FText::AsNumber(CachedBuildingCount), NSLOCTEXT("HQ", "BuildingUnit", "개"));
	if (Kpi_Employees) Kpi_Employees->SetValue(FText::AsNumber(CachedTotalEmployees), NSLOCTEXT("HQ", "EmployeeUnit", "명"));

	if (!IndustryCardBox) return;
	IndustryCardBox->ClearChildren();

	USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>();
	UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	USaveGame_GameData* SaveData = SaveMgr ? SaveMgr->GetCurrentSaveData() : nullptr;
	if (!TableMgr || !SaveData) return;

	TSubclassOf<UUserWidget> CardClass = TableMgr->GetWidgetClass(EWidgetType::HQIndustryCard);
	if (!CardClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[HQManage] DT_WidgetClass row missing for HQIndustryCard"));
		return;
	}

	struct FIndustryEntry
	{
		FCompanyInfoTable Info;
		FHQIndustryMetrics Metrics;
	};
	TArray<FIndustryEntry> Industries;
	for (int32 TypeValue = static_cast<int32>(ECompanyType::None) + 1;
		TypeValue < static_cast<int32>(ECompanyType::Max); ++TypeValue)
	{
		bool bFound = false;
		const FCompanyInfoTable Info = TableMgr->GetCompanyInfo(static_cast<ECompanyType>(TypeValue), bFound);
		if (!bFound)
		{
			UE_LOG(LogTemp, Warning, TEXT("[HQManage] Missing DT_CompanyInfo row for company type %d"), TypeValue);
			continue;
		}
		if (!Info.bAvailableAtLaunch) continue;
		Industries.Add({Info, FHQIndustryMetrics()});
	}

	// 본사 정보 갱신 시점에만 업종별 전체 로스터를 집계한다. 출장 중인 직원도 저장 로스터에 포함된다.
	UProjectOperationManager* OpMgr = GetGameInstance()->GetSubsystem<UProjectOperationManager>();
	for (const FBuildingEntitySaveData& Building : SaveData->GameData.Buildings)
	{
		const ECompanyType BuildingType = Building.BuildingData.CompanyType;
		FIndustryEntry* Entry = Industries.FindByPredicate(
			[BuildingType](const FIndustryEntry& Candidate) { return Candidate.Info.CompanyType == BuildingType; });
		if (!Entry) continue;

		++Entry->Metrics.Buildings;
		Entry->Metrics.HighestTier = FMath::Max(Entry->Metrics.HighestTier, SaveMgr->GetBuildingTier(Building.BuildingIndex));
		if (const FOfficeSaveData* OfficeData = SaveData->GameData.OfficeDataMap.Find(Building.BuildingIndex))
		{
			Entry->Metrics.Employees += OfficeData->EmployeeList.Num();
		}
		// 빌딩 목록 카드와 같은 표시 수익(감쇠O/진동X)의 합 — 새 지표를 만들지 않는다
		if (OpMgr)
		{
			Entry->Metrics.NetPerSec += OpMgr->GetBuildingDisplayNetPerSec(Building.BuildingIndex);
		}
	}

	Industries.Sort([](const FIndustryEntry& A, const FIndustryEntry& B)
	{
		return A.Info.RequiredHQLevel == B.Info.RequiredHQLevel
			? static_cast<uint8>(A.Info.CompanyType) < static_cast<uint8>(B.Info.CompanyType)
			: A.Info.RequiredHQLevel < B.Info.RequiredHQLevel;
	});

	int32 EnteredCount = 0;
	for (int32 CardIndex = 0; CardIndex < Industries.Num(); ++CardIndex)
	{
		const FIndustryEntry& Entry = Industries[CardIndex];
		const bool bLocked = CachedHQLevel < Entry.Info.RequiredHQLevel;
		const EHQIndustryState State = bLocked ? EHQIndustryState::Locked
			: Entry.Metrics.Buildings > 0 ? EHQIndustryState::Operating
			: EHQIndustryState::NotEntered;
		EnteredCount += State == EHQIndustryState::Operating ? 1 : 0;

		UHQIndustryCardWidget* Card = CreateWidget<UHQIndustryCardWidget>(this, CardClass);
		if (!Card) continue;
		Card->SetIndustry(Entry.Info, State, Entry.Metrics);
		if (UHorizontalBoxSlot* CardSlot = IndustryCardBox->AddChildToHorizontalBox(Card))
		{
			CardSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			CardSlot->SetPadding(FMargin(CardIndex == 0 ? 0.f : 12.f, 0.f, CardIndex == Industries.Num() - 1 ? 0.f : 12.f, 0.f));
		}
	}

	if (IndustryCountText)
	{
		IndustryCountText->SetText(FText::Format(NSLOCTEXT("HQ", "IndustryCount", "진출 {0} / {1}"),
			FText::AsNumber(EnteredCount), FText::AsNumber(Industries.Num())));
	}
}

void UHQManagePanelWidget::RefreshAffordability(int64 CurrentMoney)
{
	// 돈 의존 표시만 갱신 — OnResourceChanged는 고빈도라 전체 UpdateHQInfo 금지
	if (Kpi_Money) Kpi_Money->SetValue(UGlobalUtilFunctions::AbbreviateNumber(CurrentMoney), FText::GetEmpty());
	if (!bNextLevelAvailable) return;
	if (LevelUpButton)
		LevelUpButton->RefreshAffordability(EResourceType::Money, CurrentMoney);
	UpdateCurrencyCondition(Row_Cond_Money, NSLOCTEXT("HQ", "MoneyConditionName", "보유 자금"), CurrentMoney, CachedNextData.MoneyCost);
	UpdateOverallProgress(CurrentMoney);
	UpdateLevelUpButtonState();
}

void UHQManagePanelWidget::UpdateCountCondition(UHQConditionCardWidget* Card, const FText& Label, int32 Current, int32 Target, const FText& Unit)
{
	if (!Card) return;
	if (!bNextLevelAvailable || Target <= 0)
	{
		Card->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	Card->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	auto WithUnit = [&Unit](int32 Value) { return FText::Format(NSLOCTEXT("HQ", "CountWithUnit", "{0}{1}"), FText::AsNumber(Value), Unit); };
	const bool bMet = Current >= Target;
	Card->SetCondition(Label, WithUnit(Current), WithUnit(Target),
		static_cast<float>(Current) / static_cast<float>(Target), bMet,
		FText::Format(NSLOCTEXT("HQ", "ConditionMissing", "{0} 더 필요"), WithUnit(Target - FMath::Max(Current, 0))));
}

void UHQManagePanelWidget::UpdateCurrencyCondition(UHQConditionCardWidget* Card, const FText& Label, int64 Current, int64 Target)
{
	if (!Card) return;
	if (!bNextLevelAvailable || Target <= 0)
	{
		Card->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	Card->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	const bool bMet = Current >= Target;
	const double Ratio = static_cast<double>(Current) / static_cast<double>(Target);
	const FText Missing = UGlobalUtilFunctions::AbbreviateNumber(Target - FMath::Max<int64>(Current, 0), ENumberAbbrevStyle::Auto, ENumberRoundMode::Ceil);
	Card->SetCondition(Label,
		UGlobalUtilFunctions::AbbreviateNumber(Current),
		UGlobalUtilFunctions::AbbreviateNumber(Target, ENumberAbbrevStyle::Auto, ENumberRoundMode::Ceil),
		static_cast<float>(FMath::Clamp(Ratio, 0.0, 1.0)), bMet,
		FText::Format(NSLOCTEXT("HQ", "ConditionMissing", "{0} 더 필요"), Missing));
}

void UHQManagePanelWidget::UpdateOverallProgress(int64 CurrentMoney)
{
	const float Progress = bNextLevelAvailable ? ComputeOverallProgress(CurrentMoney) : 1.f;
	const int32 Unmet = bNextLevelAvailable ? ComputeUnmetCount(CurrentMoney) : 0;
	const bool bAllMet = Unmet == 0;

	if (OverallProgressBar)
	{
		OverallProgressBar->SetPercent(Progress);
		OverallProgressBar->SetFillColorAndOpacity(bAllMet ? ProgressMetColor : ProgressPendingColor);
	}
	if (OverallProgressText)
	{
		OverallProgressText->SetText(bNextLevelAvailable
			? FText::FromString(FString::Printf(TEXT("%d%%"), bAllMet ? 100 : FMath::Min(99, FMath::FloorToInt(Progress * 100.f))))
			: NSLOCTEXT("HQ", "HQMaxLevel", "MAX"));
		OverallProgressText->SetColorAndOpacity(FSlateColor(bNextLevelAvailable ? HeroPercentInk : HeroMaxInk));
	}

	if (HeroStatusText)
	{
		if (!bNextLevelAvailable)
		{
			HeroStatusText->SetText(NSLOCTEXT("HQ", "HeroAllLevels", "모든 레벨을 달성했습니다"));
		}
		else if (bAllMet)
		{
			HeroStatusText->SetText(NSLOCTEXT("HQ", "HeroReady", "조건을 모두 달성했습니다. 지금 레벨업할 수 있습니다"));
		}
		else
		{
			int32 Total = 0;
			Total += CachedNextData.MoneyCost > 0;
			Total += CachedNextData.RequiredBuildingCount > 0;
			Total += CachedNextData.RequiredTier > 0 && CachedNextData.RequiredTierBuildingCount > 0;
			Total += CachedNextData.RequiredEmployeeCount > 0;
			Total += CachedNextData.RequiredMarketCap > 0;
			HeroStatusText->SetText(FText::Format(NSLOCTEXT("HQ", "HeroConditionCount", "조건 {0} / {1} 달성"),
				FText::AsNumber(Total - Unmet), FText::AsNumber(Total)));
		}
		HeroStatusText->SetColorAndOpacity(FSlateColor(bAllMet ? HeroReadyInk : HeroStatusInk));
	}
	if (HeroMissingText)
	{
		HeroMissingText->SetVisibility(bAllMet ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
		if (!bAllMet)
		{
			HeroMissingText->SetText(BuildMissingConditionNames(CurrentMoney));
		}
	}
}

float UHQManagePanelWidget::ComputeOverallProgress(int64 CurrentMoney) const
{
	// 활성 조건(목표>0)의 정규화 진행률 평균 — 이산 충족 카운트가 아니라 "거의 다 모은 돈"이 보이게
	float Sum = 0.f;
	int32 Count = 0;
	auto Acc = [&Sum, &Count](double Cur, double Target)
	{
		if (Target <= 0.0) return;
		Sum += static_cast<float>(FMath::Clamp(Cur / Target, 0.0, 1.0));
		++Count;
	};
	Acc(static_cast<double>(CurrentMoney), static_cast<double>(CachedNextData.MoneyCost));
	Acc(CachedBuildingCount, CachedNextData.RequiredBuildingCount);
	Acc(CachedTierBuildingCount, CachedNextData.RequiredTier > 0 ? CachedNextData.RequiredTierBuildingCount : 0);
	Acc(CachedTotalEmployees, CachedNextData.RequiredEmployeeCount);
	Acc(static_cast<double>(CachedMarketCap), static_cast<double>(CachedNextData.RequiredMarketCap));

	return Count > 0 ? Sum / Count : 1.f;
}

int32 UHQManagePanelWidget::ComputeUnmetCount(int64 CurrentMoney) const
{
	int32 Unmet = 0;
	auto Check = [&Unmet](double Cur, double Target) { if (Target > 0.0 && Cur < Target) ++Unmet; };
	Check(static_cast<double>(CurrentMoney), static_cast<double>(CachedNextData.MoneyCost));
	Check(CachedBuildingCount, CachedNextData.RequiredBuildingCount);
	Check(CachedTierBuildingCount, CachedNextData.RequiredTier > 0 ? CachedNextData.RequiredTierBuildingCount : 0);
	Check(CachedTotalEmployees, CachedNextData.RequiredEmployeeCount);
	Check(static_cast<double>(CachedMarketCap), static_cast<double>(CachedNextData.RequiredMarketCap));
	return Unmet;
}

FText UHQManagePanelWidget::BuildMissingConditionNames(int64 CurrentMoney) const
{
	// 카드 라벨과 같은 이름을 같은 순서로 나열한다
	TArray<FText> Names;
	auto Add = [&Names](double Cur, double Target, const FText& Name) { if (Target > 0.0 && Cur < Target) Names.Add(Name); };
	Add(static_cast<double>(CurrentMoney), static_cast<double>(CachedNextData.MoneyCost), NSLOCTEXT("HQ", "MoneyConditionName", "보유 자금"));
	Add(CachedBuildingCount, CachedNextData.RequiredBuildingCount, NSLOCTEXT("HQ", "BuildingConditionName", "보유 빌딩"));
	Add(CachedTierBuildingCount, CachedNextData.RequiredTier > 0 ? CachedNextData.RequiredTierBuildingCount : 0,
		FText::Format(NSLOCTEXT("HQ", "TierCondition", "{0}단계 이상 빌딩"), FText::AsNumber(CachedNextData.RequiredTier)));
	Add(CachedTotalEmployees, CachedNextData.RequiredEmployeeCount, NSLOCTEXT("HQ", "EmployeeConditionName", "전체 직원"));
	Add(static_cast<double>(CachedMarketCap), static_cast<double>(CachedNextData.RequiredMarketCap), NSLOCTEXT("HQ", "MarketCapConditionName", "시가총액"));
	return FText::Format(NSLOCTEXT("HQ", "HeroMissing", "부족: {0}"),
		FText::Join(NSLOCTEXT("HQ", "HeroMissingSeparator", " · "), Names));
}

void UHQManagePanelWidget::UpdateLevelUpButtonState()
{
	if (!LevelUpButton) return;

	if (!bNextLevelAvailable)
	{
		LevelUpButton->SetMaxLevelState(true);
		LevelUpButton->SetCostVisible(false);
		LevelUpButton->SetLockedState(true);
		bCanLevelUpNow = false;
		bCTAShineActive = false;
		if (CTAShineImage) CTAShineImage->SetRenderOpacity(0.f);
		UpdateGrowthTabAlert();
		return;
	}

	USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>();
	bCanLevelUpNow = SaveMgr && SaveMgr->CanLevelUpHQ();

	LevelUpButton->SetVisibility(ESlateVisibility::Visible);
	LevelUpButton->SetMaxLevelState(false);
	LevelUpButton->SetCostVisible(CachedNextData.MoneyCost > 0);
	LevelUpButton->SetLockedState(!bCanLevelUpNow);
	bCTAShineActive = bCanLevelUpNow;  // 레디 어포던스 = 샤인 스윕만 (펄스 금지 — v4)
	UpdateGrowthTabAlert();
}

void UHQManagePanelWidget::UpdateGrowthTabAlert()
{
	if (!GrowthTabAlert) return;
	const bool bOnGrowth = CurrentTabIndex == GrowthTabIndex;
	GrowthTabAlert->SetVisibility(bCanLevelUpNow && !bOnGrowth ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UHQManagePanelWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!ContentSwitcher || ContentSwitcher->GetActiveWidgetIndex() != GrowthTabIndex) return;
	if (!CTAShineImage && !LevelBadgeGloss) return;

	if (USettingsManagerSubsystem::IsReduceMotion(this))
	{
		for (UImage* ImageWidget : {LevelBadgeGloss.Get(), CTAShineImage})
		{
			if (ImageWidget) ImageWidget->SetRenderOpacity(0.f);
		}
		return;
	}

	VFXTimer += InDeltaTime;
	const float BadgePeriod = bCTAShineActive ? 2.6f : 6.f;
	const float BadgePhase = FMath::Fmod(VFXTimer, BadgePeriod);
	constexpr float BadgeTravel = 0.65f;
	if (LevelBadgeGloss)
	{
		if (BadgePhase < BadgeTravel)
		{
			const float T = BadgePhase / BadgeTravel;
			LevelBadgeGloss->SetRenderTranslation(FVector2D(FMath::Lerp(-72.f, 72.f, T), 0.f));
			LevelBadgeGloss->SetRenderOpacity(0.32f * FMath::Sin(T * PI));
		}
		else LevelBadgeGloss->SetRenderOpacity(0.f);
	}

	if (CTAShineImage)
	{
		if (bCTAShineActive)
		{
			const float Phase = FMath::Fmod(VFXTimer, ShinePeriod);
			if (Phase < ShineTravel)
			{
				const float T = Phase / ShineTravel;
				CTAShineImage->SetRenderTranslation(FVector2D(FMath::Lerp(-ShineSweepHalf, ShineSweepHalf, T), 0.f));
				CTAShineImage->SetRenderOpacity(0.6f * FMath::Sin(T * PI));
			}
			else CTAShineImage->SetRenderOpacity(0.f);
		}
		else CTAShineImage->SetRenderOpacity(0.f);
	}
}

void UHQManagePanelWidget::PopulateBuildingList()
{
	if (!BuildingListScrollBox) return;
	BuildingListScrollBox->ClearChildren();

	UEntityManager* EntityMgr = GetWorld()->GetSubsystem<UEntityManager>();
	if (!EntityMgr) return;

	UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	if (!TableMgr) return;

	TSubclassOf<UUserWidget> SlotClass = TableMgr->GetWidgetClass(EWidgetType::HQBuildingSlot);
	if (!SlotClass) return;

	TArray<ABuildingBaseActor*> Buildings;
	for (ABuildingBaseActor* Building : EntityMgr->GetBuildings())
	{
		if (Building) Buildings.Add(Building);
	}
	if (ListHeaderText)
		ListHeaderText->SetText(FText::Format(NSLOCTEXT("HQ", "BuildingListCount", "보유 빌딩 {0}개"), FText::AsNumber(Buildings.Num())));

	// TArray::Sort는 unstable — 동률은 BuildingIndex 오름차순으로 순서 고정
	const bool bAsc = bBuildingSortAscending;
	const EHQBuildingSortMode Mode = BuildingSortMode;
	UProjectOperationManager* OpMgr = GetGameInstance()->GetSubsystem<UProjectOperationManager>();
	UEmployeeManager* EmpMgr = GetGameInstance()->GetSubsystem<UEmployeeManager>();
	USaveLoadManager* TierSaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>();
	Buildings.Sort([bAsc, Mode, OpMgr, EmpMgr, TierSaveMgr](const ABuildingBaseActor& A, const ABuildingBaseActor& B)
	{
		double KeyA = 0.0;
		double KeyB = 0.0;
		switch (Mode)
		{
		case EHQBuildingSortMode::Income:
		{
			// 카드 표시와 동일 기준 = 표시 순수익(감쇠O/진동X). 진동 스냅샷 정렬은 매초 순서가 흔들려 부적합
			KeyA = OpMgr ? OpMgr->GetBuildingDisplayNetPerSec(A.GetBuildingIndex()) : 0.0;
			KeyB = OpMgr ? OpMgr->GetBuildingDisplayNetPerSec(B.GetBuildingIndex()) : 0.0;
			break;
		}
		case EHQBuildingSortMode::Employees:
			KeyA = EmpMgr ? EmpMgr->GetEmployeeCountInBuilding(A.GetBuildingIndex()) : 0;
			KeyB = EmpMgr ? EmpMgr->GetEmployeeCountInBuilding(B.GetBuildingIndex()) : 0;
			break;
		case EHQBuildingSortMode::Level:
		default:
			KeyA = TierSaveMgr ? TierSaveMgr->GetBuildingTier(A.GetBuildingIndex()) : 0;
			KeyB = TierSaveMgr ? TierSaveMgr->GetBuildingTier(B.GetBuildingIndex()) : 0;
			break;
		}
		if (KeyA != KeyB)
		{
			return bAsc ? KeyA < KeyB : KeyA > KeyB;
		}
		return A.GetBuildingIndex() < B.GetBuildingIndex();
	});

	for (ABuildingBaseActor* Building : Buildings)
	{
		UBuildingSlotHorizonWidget* SlotWidget = CreateWidget<UBuildingSlotHorizonWidget>(this, SlotClass);
		if (!SlotWidget) continue;

		SlotWidget->SetBuildingData(Building);
		SlotWidget->OnMoveClicked.BindUObject(this, &UHQManagePanelWidget::OnBuildingMoveClicked);
		BuildingListScrollBox->AddChild(SlotWidget);

		// 행 간격 — WBP 슬롯이 아니라 런타임 AddChild라 코드가 줘야 함 (목업 18)
		if (UScrollBoxSlot* CardSlot = Cast<UScrollBoxSlot>(SlotWidget->Slot))
		{
			CardSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 18.f));
		}
	}
}

void UHQManagePanelWidget::SetupSortButtonGroup()
{
	SortButtonGroup = NewObject<UCommonButtonGroupBase>(this);
	if (!SortButtonGroup) return;

	SortButtonGroup->SetSelectionRequired(true);

	// SetIsInteractableWhenSelected(true) 필수 — selected 버튼 재클릭 OnPressed 발화가 방향 토글의 전제 (ProductSellModal 선례)
	auto Setup = [this](UButtonWidget* Btn, void (UHQManagePanelWidget::*Handler)())
	{
		if (!Btn) return;
		Btn->SetIsSelectable(true);
		Btn->SetIsInteractableWhenSelected(true);
		SortButtonGroup->AddWidget(Btn);
		Btn->OnPressed().AddUObject(this, Handler);
	};
	Setup(SortLevelBtn,    &UHQManagePanelWidget::HandleSortLevelClicked);
	Setup(SortIncomeBtn,   &UHQManagePanelWidget::HandleSortIncomeClicked);
	Setup(SortEmployeeBtn, &UHQManagePanelWidget::HandleSortEmployeeClicked);
	SortButtonGroup->SelectButtonAtIndex(0);  // 단계순 기본
	UpdateSortButtonTexts();
}

void UHQManagePanelWidget::HandleSortLevelClicked()
{
	if (BuildingSortMode == EHQBuildingSortMode::Level) { bBuildingSortAscending = !bBuildingSortAscending; }
	else { BuildingSortMode = EHQBuildingSortMode::Level; bBuildingSortAscending = false; }
	UpdateSortButtonTexts();
	PopulateBuildingList();
}

void UHQManagePanelWidget::HandleSortIncomeClicked()
{
	if (BuildingSortMode == EHQBuildingSortMode::Income) { bBuildingSortAscending = !bBuildingSortAscending; }
	else { BuildingSortMode = EHQBuildingSortMode::Income; bBuildingSortAscending = false; }
	UpdateSortButtonTexts();
	PopulateBuildingList();
}

void UHQManagePanelWidget::HandleSortEmployeeClicked()
{
	if (BuildingSortMode == EHQBuildingSortMode::Employees) { bBuildingSortAscending = !bBuildingSortAscending; }
	else { BuildingSortMode = EHQBuildingSortMode::Employees; bBuildingSortAscending = false; }
	UpdateSortButtonTexts();
	PopulateBuildingList();
}

void UHQManagePanelWidget::UpdateSortButtonTexts()
{
	// 오름차순 ▲ / 내림차순 ▼ — selected 버튼만 방향 표시 (ProductSellModal 컨벤션)
	auto ApplyText = [this](UButtonWidget* Btn, EHQBuildingSortMode Mode, const FText& Label)
	{
		if (!Btn) return;
		const FString Suffix = (BuildingSortMode == Mode)
			? (bBuildingSortAscending ? FString(TEXT(" ▲")) : FString(TEXT(" ▼")))
			: FString();
		Btn->SetButtonText(FText::FromString(Label.ToString() + Suffix));
	};
	ApplyText(SortLevelBtn,    EHQBuildingSortMode::Level,     NSLOCTEXT("HQ", "SortByTier", "단계순"));
	ApplyText(SortIncomeBtn,   EHQBuildingSortMode::Income,    NSLOCTEXT("HQ", "SortByIncome", "수익순"));
	ApplyText(SortEmployeeBtn, EHQBuildingSortMode::Employees, NSLOCTEXT("HQ", "SortByEmployees", "직원순"));
}

void UHQManagePanelWidget::OnBuildingMoveClicked(int32 BuildingIndex)
{
	UEntityManager* EntityMgr = GetWorld()->GetSubsystem<UEntityManager>();
	if (!EntityMgr) return;

	ABuildingBaseActor* Building = EntityMgr->GetBuildingByIndex(BuildingIndex);
	if (!Building) return;

	CloseWithAnimation();

	// 카메라를 해당 건물로 포커스
	AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(GetWorld()->GetFirstPlayerController());
	if (PC)
	{
		APlayerCamera* Camera = Cast<APlayerCamera>(PC->GetPawn());
		if (Camera)
		{
			Camera->FocusOnActor(Building, 0.35f);
		}
	}

	// 빌딩 관리 패널 열기
	UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	UUIManagerSubsystem* UIManager = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
	if (!TableMgr || !UIManager) return;

	TSubclassOf<UUserWidget> ManageClass = TableMgr->GetWidgetClass(EWidgetType::BuildingManage);
	UCommonActivatableWidget* Widget = UIManager->GetUIBase()->PushBottomClass(ManageClass.Get());

	if (UBuildingManagePanelWidget* ManagePanel = Cast<UBuildingManagePanelWidget>(Widget))
	{
		ManagePanel->SetTargetBuilding(Building);
		if (PC) PC->GoToUIMode();

		// 후임이 실제로 열렸을 때만 인계 표시 — push 실패 시엔 이 패널이 정상대로 모드/고스트를 원복해야 한다
		bHandingOffToManagePanel = true;
	}
}

void UHQManagePanelWidget::OnCloseButtonClicked()
{
	CloseWithAnimation();
}

void UHQManagePanelWidget::HandleHQLevelUp(int32 NewLevel)
{
	UpdateHQInfo();
	if (IsActivated() && GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(PanelIntroTimer, this,
			&UHQManagePanelWidget::TryPlayPanelIntro, 0.5f, false);
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (USoundManagerSubsystem* SM = GI->GetSubsystem<USoundManagerSubsystem>())
		{
			SM->PlayUISound(CGUISoundTags::RewardCompanyLevelUp);
		}
	}
}

void UHQManagePanelWidget::HandleStageComplete()
{
	UpdateHQInfo();
}

void UHQManagePanelWidget::HandleResourceChanged(EResourceType Type, int64 NewValue)
{
	if (Type == EResourceType::Money)
	{
		RefreshAffordability(NewValue);
	}
	else if (Type == EResourceType::MarketCap)
	{
		CachedMarketCap = NewValue;
		if (Kpi_MarketCap) Kpi_MarketCap->SetValue(UGlobalUtilFunctions::AbbreviateNumber(NewValue), FText::GetEmpty());
		if (!bNextLevelAvailable) return;
		UpdateCurrencyCondition(Row_Cond_MarketCap, NSLOCTEXT("HQ", "MarketCapConditionName", "시가총액"), NewValue, CachedNextData.RequiredMarketCap);
		int64 CurrentMoney = 0;
		if (UResourceItemManager* ResMgr = GetGameInstance()->GetSubsystem<UResourceItemManager>())
			CurrentMoney = ResMgr->GetResourceAmount(EResourceType::Money);
		UpdateOverallProgress(CurrentMoney);
		UpdateLevelUpButtonState();
	}
}

void UHQManagePanelWidget::HandleEmployeeHireCompleted(const FString& EmployeeID)
{
	UpdateHQInfo();
}

FText UHQManagePanelWidget::BuildLevelRewardText(int32 Level) const
{
	UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr) return FText::GetEmpty();

	bool bOk = false;
	const FHQLevelData Data = TableMgr->GetHQLevelData(Level, bOk);
	if (!bOk) return FText::GetEmpty();

	if (!Data.UnlockDescription.IsEmpty()) return Data.UnlockDescription;

	// 빌딩 1종만 여는 레벨은 문구를 비워 둔다 — 이름을 DT 두 곳에 적지 않고 빌딩 표에서 읽는다
	TArray<FHQRewardSlot> Rewards;
	CollectRewardSlots(Level, Rewards);
	for (const FHQRewardSlot& Reward : Rewards)
	{
		if (Reward.Style == EHQRewardSlotStyle::Building && Reward.Count.IsEmpty())
		{
			return FText::Format(NSLOCTEXT("HQ", "BuildingUnlockSummary", "{0} 해금"), Reward.Caption);
		}
	}
	return FText::GetEmpty();
}

void UHQManagePanelWidget::PopulateRewardSlots(int32 Level)
{
	if (!RewardSlotBox) return;
	RewardSlotBox->ClearChildren();

	UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	TSubclassOf<UUserWidget> SlotClass;
	if (TableMgr) SlotClass = TableMgr->GetWidgetClass(EWidgetType::HQRewardSlot);
	if (!SlotClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[HQManage] DT_WidgetClass row missing for HQRewardSlot"));
		return;
	}

	TArray<FHQRewardSlot> Rewards;
	CollectRewardSlots(Level, Rewards);
	if (Rewards.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[HQManage] No reward slots derived for HQ Lv.%d"), Level);
	}
	// 넓은 간격은 칸 두 개일 때만 — 큰 칸 세 개는 넓은 간격이면 한 줄에 못 들어가 둘째 줄로 넘친다
	const FVector2D Gap = Rewards.Num() == 2 ? FewRewardSlotGap : RewardSlotGap;
	RewardSlotBox->SetInnerSlotPadding(Gap);
	for (const FHQRewardSlot& Reward : Rewards)
	{
		UHQRewardSlotWidget* RewardWidget = CreateWidget<UHQRewardSlotWidget>(this, SlotClass);
		if (!RewardWidget) continue;
		RewardWidget->SetReward(Reward, Rewards.Num(), Gap.X);
		RewardSlotBox->AddChildToWrapBox(RewardWidget);
	}
}

void UHQManagePanelWidget::CollectRewardSlots(int32 Level, TArray<FHQRewardSlot>& OutSlots) const
{
	UGameInstance* GI = GetGameInstance();
	USaveLoadManager* SaveMgr = GI ? GI->GetSubsystem<USaveLoadManager>() : nullptr;
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!SaveMgr || !TableMgr) return;

	// 업종 — 지급과 같은 목록이라 채용권·랜덤 보상 수도 여기서 나온다
	const TArray<ECompanyType> Industries = HQLevelUpReward::GetIndustriesUnlockedAt(*SaveMgr, Level);
	for (const ECompanyType Type : Industries)
	{
		bool bFound = false;
		const FCompanyInfoTable Info = TableMgr->GetCompanyInfo(Type, bFound);
		if (!bFound) continue;
		FHQRewardSlot& Industry = OutSlots.AddDefaulted_GetRef();
		Industry.Style = EHQRewardSlotStyle::Industry;
		Industry.Icon = Info.PanelIcon.IsNull() ? nullptr : Info.PanelIcon.LoadSynchronous();
		Industry.Caption = Info.DisplayName;
		Industry.Accent = Info.AccentColor;
	}

	// 공장 — 자동(오프라인) 묶음의 해금 레벨 (BrickFactory 게이트와 같은 정책)
	for (const FFactoryUpgradeDefinition& Definition : TableMgr->GetAllFactoryUpgradeDefinitions())
	{
		if (!FactoryUpgradeUnlockPolicy::IsAutoUnlockType(Definition.UpgradeType) || Definition.RequiredHQLevel != Level) continue;
		FHQRewardSlot& Factory = OutSlots.AddDefaulted_GetRef();
		Factory.Style = EHQRewardSlotStyle::Feature;
		Factory.Icon = Definition.Icon.IsNull() ? nullptr : Definition.Icon.LoadSynchronous();
		Factory.Caption = Definition.DisplayName;
	}

	if (Level == WorldMapConstants::WorldMapRequiredHQLevel)
	{
		FHQRewardSlot& WorldMap = OutSlots.AddDefaulted_GetRef();
		WorldMap.Style = EHQRewardSlotStyle::Feature;
		WorldMap.Icon = WorldMapRewardIcon;
		WorldMap.Caption = NSLOCTEXT("HQ", "WorldMapRewardSlot", "세계지도");
	}

	// 빌딩 — 새로 지을 수 있는 종류를 칸 수별로 묶는다. 한 종이면 그 빌딩 이름, 여럿이면 "N칸 빌딩 M종"(그림 = 표 순서상 첫 빌딩)
	struct FBuildingGroup
	{
		int32 Cells = 0;
		int32 Kinds = 0;
		FName FirstName;
		TSoftObjectPtr<UTexture2D> Icon;
	};
	TArray<FBuildingGroup> Groups;
	for (const FBuildableCardTable& Card : TableMgr->GetBuildableInfos())
	{
		bool bFound = false;
		const FBuildingData Data = TableMgr->GetBuildingData(Card.RowName, bFound);
		if (!bFound || Data.RequiredHQLevel != Level) continue;
		const int32 Cells = Data.FootprintWidthCells * Data.FootprintDepthCells;
		FBuildingGroup* Group = Groups.FindByPredicate([Cells](const FBuildingGroup& Candidate) { return Candidate.Cells == Cells; });
		if (!Group)
		{
			Group = &Groups.AddDefaulted_GetRef();
			Group->Cells = Cells;
			Group->FirstName = Card.Name;
			Group->Icon = Card.UIIcon;
		}
		++Group->Kinds;
	}
	Groups.Sort([](const FBuildingGroup& A, const FBuildingGroup& B) { return A.Cells < B.Cells; });
	for (const FBuildingGroup& Group : Groups)
	{
		FHQRewardSlot& Building = OutSlots.AddDefaulted_GetRef();
		Building.Style = EHQRewardSlotStyle::Building;
		Building.Icon = Group.Icon.IsNull() ? nullptr : Group.Icon.LoadSynchronous();
		if (Group.Kinds == 1)
		{
			Building.Caption = FText::FromName(Group.FirstName);
			continue;
		}
		Building.Caption = FText::Format(NSLOCTEXT("HQ", "BuildingKindSlot", "{0}칸 빌딩"), FText::AsNumber(Group.Cells));
		Building.Count = FText::Format(NSLOCTEXT("HQ", "BuildingKindCount", "{0}종"), FText::AsNumber(Group.Kinds));
	}

	// 메뉴 — DT_MenuUnlock 행 이름 = 메뉴 시트 타일 이름이라, 메뉴 시트에 저작된 라벨·아이콘을 그대로 쓴다
	const UWidgetBlueprintGeneratedClass* MenuClass = Cast<UWidgetBlueprintGeneratedClass>(TableMgr->GetWidgetClass(EWidgetType::MenuPanel).Get());
	if (const UWidgetTree* MenuTree = MenuClass ? MenuClass->GetWidgetTreeArchetype() : nullptr)
	{
		MenuTree->ForEachWidget([&OutSlots, TableMgr, Level](UWidget* Widget)
		{
			const UMenuTileWidget* Tile = Cast<UMenuTileWidget>(Widget);
			if (!Tile) return;
			bool bFound = false;
			const FMenuUnlockData Unlock = TableMgr->GetMenuUnlockData(Tile->GetFName(), bFound);
			if (!bFound || Unlock.RequiredHQLevel != Level) return;
			FHQRewardSlot& Menu = OutSlots.AddDefaulted_GetRef();
			Menu.Style = EHQRewardSlotStyle::Menu;
			Menu.Icon = Tile->GetTileIcon();
			Menu.Caption = Tile->GetTileLabel();
		});
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[HQManage] Menu panel widget tree missing — menu unlock rewards hidden"));
	}

	// 아이템 — 지급 규칙(HQLevelUpReward)에 있는 것만, 지급 순서대로
	auto AddItemSlot = [&OutSlots, GI](EItemType ItemType, int64 Quantity)
	{
		CGRewardChip::FDisplayEntry Entry;
		Entry.ItemType = ItemType;
		Entry.Quantity = Quantity;
		FHQRewardSlot& ItemSlot = OutSlots.AddDefaulted_GetRef();
		ItemSlot.Icon = CGRewardChip::ResolveIcon(GI, Entry);
		ItemSlot.Caption = GetItemTypeDisplayName(ItemType);
		ItemSlot.Count = CGRewardChip::FormatQuantity(Entry);
	};
	if (Industries.Num() > 0)
	{
		AddItemSlot(HQLevelUpReward::IndustryTicketType, Industries.Num() * HQLevelUpReward::IndustryTicketAmount);

		FHQRewardSlot& LootSlot = OutSlots.AddDefaulted_GetRef();
		LootSlot.Icon = RandomRewardIcon;
		LootSlot.Caption = NSLOCTEXT("HQ", "RandomRewardSlot", "랜덤 보상");
		LootSlot.Count = FText::Format(NSLOCTEXT("Reward", "Qty", "x{0}"), FText::AsNumber(Industries.Num() * HQLevelUpReward::IndustryLootRolls));
	}
	AddItemSlot(HQLevelUpReward::LevelTraitTicketType, HQLevelUpReward::LevelTraitTicketAmount);
	if (HQLevelUpReward::IsMilestoneLevel(Level))
	{
		AddItemSlot(HQLevelUpReward::MilestoneTraitTicketType, HQLevelUpReward::MilestoneTraitTicketAmount);
	}
}

void UHQManagePanelWidget::OnLevelUpButtonClicked()
{
	USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>();
	UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	if (!SaveMgr || !TableMgr) return;

	int32 OldLevel = SaveMgr->GetHQLevel();

	if (SaveMgr->TryLevelUpHQ())
	{
		int32 NewLevel = SaveMgr->GetHQLevel();

		const FText RewardText = BuildLevelRewardText(NewLevel);

		TSubclassOf<UUserWidget> CelebClass = TableMgr->GetWidgetClass(EWidgetType::HQLevelUpCelebration);
		if (CelebClass)
		{
			UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
			if (UIMgr && UIMgr->GetUIBase())
			{
				UCommonActivatableWidget* Widget = UIMgr->GetUIBase()->PushPromptClass(CelebClass.Get());
				if (UHQLevelUpCelebrationWidget* Celeb = Cast<UHQLevelUpCelebrationWidget>(Widget))
				{
					Celeb->SetLevelUpInfo(OldLevel, NewLevel, RewardText);
				}
			}
		}
		// 패널 갱신은 OnHQLevelUp → HandleHQLevelUp이 처리
	}
	else
	{
		// 자금 부족 등으로 레벨업 실패 — '무반응' 방지 (claim의 이중 안전망)
		if (USoundManagerSubsystem* SM = GetGameInstance()->GetSubsystem<USoundManagerSubsystem>())
		{
			SM->PlayUISound(CGUISoundTags::PurchaseFail);
		}
		if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
		{
			UIMgr->ShowNotification(NSLOCTEXT("HQ", "HQInsufficient", "자금이 부족합니다"), 2.0f, ENotificationType::Failed);
			if (UInGameLayerWidget* InGame = UIMgr->GetInGameLayer())
			{
				if (UResourceWidget* MoneyW = InGame->GetResourceWidget(EResourceType::Money))
				{
					MoneyW->PlayAffordShake();
				}
			}
		}
	}
}
