// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Panel/WorkstationInfoWidget.h"
#include "UI/Element/Common/PanelHeaderWidget.h"
#include "Manager/PanelIntroSubsystem.h"
#include "TimerManager.h"
#include "UI/Panel/OfficeMainWidget.h"
#include "UI/Panel/EmployeeWindowWidget.h"
#include "UI/Panel/OfficeEmployeePanelWidget.h"
#include "Manager/MissionManagerSubsystem.h"
#include "Office/WorkstationActorBase.h"
#include "Office/WorkstationTypes.h"
#include "Office/OfficeManager.h"
#include "Manager/EmployeeManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/UIManagerSubsystem.h"
#include "Table/WorkstationCardTable.h"
#include "Table/WorkstationTable.h"
#include "Table/VFXTable.h"
#include "Data/EmployeeTypes.h"
#include "Data/EmployeeStatsData.h"
#include "Data/EntityCardData.h"
#include "Enum/ProductionDiscipline.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "UI/UIBase.h"
#include "UI/Panel/OfficeRecruitmentModalWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Building/UpgradeSlot.h"
#include "UI/Element/Common/BulkModeSelectorWidget.h"
#include "UI/Element/Buttons/CostActionButtonWidget.h"
#include "UI/Panel/WorkstationPromotionWidget.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/SoundManagerSubsystem.h"
#include "UI/UISoundTags.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "UI/Element/Employee/EmployeeRosterCardWidget.h"
#include "Player/OfficeCameraPawn.h"
#include "Player/MainMapPlayerController.h"
#include "GameMode/OfficeGameMode.h"
#include "Entity/Officeworker/Officeworker.h"
#include "Enum/WidgetType.h"
#include "Core/CGGameInstance.h"
#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Components/Button.h"
#include "Components/ListView.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	void SetGradeCompositionImage(UImage* Image, const TSoftObjectPtr<UTexture2D>& ImageAsset)
	{
		if (!Image) return;
		UTexture2D* Texture = ImageAsset.LoadSynchronous();
		Image->SetBrushFromTexture(Texture, false);
		Image->SetVisibility(Texture ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		if (!Texture && !ImageAsset.IsNull())
		{
			UE_LOG(LogTemp, Warning, TEXT("[WorkstationInfo] Missing grade composition image: %s"),
				*ImageAsset.ToSoftObjectPath().ToString());
		}
	}

	FText BuildPromotionResultLine(const FComputerSetupLevelData& OldRow, const FComputerSetupLevelData& NewRow)
	{
		const FWorkstationEquipDelta Delta = BuildWorkstationEquipDelta(OldRow, &NewRow);
		if (Delta.ItemName.IsEmpty()) return Delta.Detail;
		if (Delta.Kind == EWorkstationEquipDeltaKind::Replaced)
		{
			return FText::Format(NSLOCTEXT("WorkstationInfo", "PromotionReplace", "{0}|hpp(으로,로) 교체"), Delta.ItemName);
		}
		const FText Added = FText::Format(NSLOCTEXT("WorkstationInfo", "PromotionAdded", "{0} 추가"), Delta.ItemName);
		return Delta.RemovedItemNames.IsEmpty()
			? Added
			: FText::Format(NSLOCTEXT("WorkstationInfo", "PromotionAddedRemoved", "{0} · {1} 제거"), Added, Delta.RemovedItemNames);
	}
}

void UWorkstationInfoWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (RelocateButton) RelocateButton->OnClicked().AddUObject(this, &ThisClass::OnRelocateClicked);

	// 미션 패널 등록 유지 — 구 도크 대상 가이드 타겟(채용/좌석/성장, 체인에서 이관됨)은 nullptr 스텁이라 링만 생략됨
	if (UMissionManagerSubsystem* MissionMgr = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>())
	{
		MissionMgr->NotifyWorkstationPanelOpened(this);
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		EmployeeManager = GI->GetSubsystem<UEmployeeManager>();
	}

	if (PromoteButton) PromoteButton->OnClicked().AddUObject(this, &ThisClass::OnDeskUpgradeClicked);
	if (ProcessingUpgradeSlot && ProcessingUpgradeSlot->GetUpgradeButton())
		ProcessingUpgradeSlot->GetUpgradeButton()->OnClicked().AddUObject(this, &ThisClass::OnProcessingUpgradeClicked);
	if (ToolsUpgradeSlot && ToolsUpgradeSlot->GetUpgradeButton())
		ToolsUpgradeSlot->GetUpgradeButton()->OnClicked().AddUObject(this, &ThisClass::OnToolsUpgradeClicked);
	if (EnvironmentUpgradeSlot && EnvironmentUpgradeSlot->GetUpgradeButton())
		EnvironmentUpgradeSlot->GetUpgradeButton()->OnClicked().AddUObject(this, &ThisClass::OnEnvironmentUpgradeClicked);
	if (BulkModeSelector)
	{
		BulkModeSelector->SetMaxModeEnabled(true);
		BulkModeSelector->OnModeChanged.AddUObject(this, &UWorkstationInfoWidget::HandleBulkModeChanged);
	}

	if (EmployeeWindowButton)
	{
		EmployeeWindowButton->OnClicked().AddUObject(this, &UWorkstationInfoWidget::OnEmployeeWindowClicked);
	}

	if (BenchListView)
	{
		BenchListView->SetSelectionMode(ESelectionMode::None);   // 탭=즉시 착석, 선택 상태 없음
		BenchListView->OnEntryWidgetGenerated().AddUObject(this, &UWorkstationInfoWidget::OnBenchEntryGenerated);
	}

	if (UnseatButton)
	{
		UnseatButton->OnClicked().AddUObject(this, &UWorkstationInfoWidget::OnUnseatClicked);
	}

	if (RecruitButton)
	{
		RecruitButton->OnClicked().AddUObject(this, &UWorkstationInfoWidget::OnRecruitClicked);
	}

	if (UCloseButtonWidget* PanelCloseButton = PanelHeader ? PanelHeader->GetCloseButton() : nullptr)
	{
		PanelCloseButton->OnCloseClicked.AddDynamic(this, &UWorkstationInfoWidget::OnCloseDelegate);
	}

	if (BackgroundBtn)
	{
		BackgroundBtn->OnClicked.AddDynamic(this, &UWorkstationInfoWidget::OnBackgroundClicked);
	}

	if (EmployeeManager)
	{
		EmployeeManager->OnEmployeeStatsChanged.AddUObject(this, &UWorkstationInfoWidget::HandleEmployeeStatsChanged);
		EmployeeManager->OnEmployeeRosterChanged.AddUObject(this, &UWorkstationInfoWidget::HandleRosterChanged);
	}
}

void UWorkstationInfoWidget::NativeDestruct()
{
	if (RelocateButton) RelocateButton->OnClicked().RemoveAll(this);
	if (PromoteButton) PromoteButton->OnClicked().RemoveAll(this);
	for (UUpgradeSlot* UpgradeSlot : {ProcessingUpgradeSlot, ToolsUpgradeSlot, EnvironmentUpgradeSlot})
	{
		if (UpgradeSlot && UpgradeSlot->GetUpgradeButton())
		{
			UpgradeSlot->GetUpgradeButton()->OnClicked().RemoveAll(this);
		}
	}
	if (BulkModeSelector)
	{
		BulkModeSelector->OnModeChanged.RemoveAll(this);
	}

	if (EmployeeWindowButton)
	{
		EmployeeWindowButton->OnClicked().RemoveAll(this);
	}

	if (BenchListView)
	{
		BenchListView->OnEntryWidgetGenerated().RemoveAll(this);
	}

	if (UnseatButton)
	{
		UnseatButton->OnClicked().RemoveAll(this);
	}

	if (RecruitButton)
	{
		RecruitButton->OnClicked().RemoveAll(this);
	}

	if (UCloseButtonWidget* PanelCloseButton = PanelHeader ? PanelHeader->GetCloseButton() : nullptr)
	{
		PanelCloseButton->OnCloseClicked.RemoveDynamic(this, &UWorkstationInfoWidget::OnCloseDelegate);
	}

	if (BackgroundBtn)
	{
		BackgroundBtn->OnClicked.RemoveDynamic(this, &UWorkstationInfoWidget::OnBackgroundClicked);
	}

	if (EmployeeManager)
	{
		EmployeeManager->OnEmployeeStatsChanged.RemoveAll(this);
		EmployeeManager->OnEmployeeRosterChanged.RemoveAll(this);
	}

	Super::NativeDestruct();
}

void UWorkstationInfoWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UResourceItemManager* ResourceMgr = GI->GetSubsystem<UResourceItemManager>())
		{
			ResourceMgr->OnResourceChanged.AddUObject(this, &UWorkstationInfoWidget::HandleResourceChanged);
		}
	}

	// 최초 진입 코치마크 — SetWorkstationData 가 활성화 뒤에 오므로 여유를 두고 시작한다
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(
			PanelIntroTimer, this, &UWorkstationInfoWidget::TryPlayPanelIntro, 0.5f, false);
	}

	// 위에 쌓인 패널이 닫혀 재활성될 때 점유/책상 상태 갱신.
	// 첫 오픈 땐 CurrentWorkstation 이 아직 null(SetWorkstationData 가 뒤) 이라 UpdateUI 는 early-return.
	UpdateUI();
}

void UWorkstationInfoWidget::TryPlayPanelIntro()
{
	// 타이머 대기 중 창이 닫혔을 수 있다
	if (!IsActivated())
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
	static const FName IntroKey(TEXT("WorkstationInfo"));
	PanelIntroOverlay = IntroMgr->PlayIntro(this, IntroKey, /*bIgnoreSeen=*/false);
	if (UPanelIntroOverlayWidget* Overlay = PanelIntroOverlay.Get())
	{
		Overlay->OnIntroFinished.AddUObject(this, &UWorkstationInfoWidget::HandlePanelIntroFinished);
	}
}

void UWorkstationInfoWidget::HandlePanelIntroFinished()
{
	PanelIntroOverlay.Reset();
}

void UWorkstationInfoWidget::NativeOnDeactivated()
{
	if (UMissionManagerSubsystem* MissionMgr = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>() : nullptr)
	{
		MissionMgr->NotifyWorkstationPanelClosed(this);
	}
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UResourceItemManager* ResourceMgr = GI->GetSubsystem<UResourceItemManager>())
		{
			ResourceMgr->OnResourceChanged.RemoveAll(this);
		}
	}
	// 패널 닫힐 때 점유 직원 오버레이도 해제
	SetOccupantHighlight(false);

	if (CurrentWorkstation)
	{
		CurrentWorkstation->SetHighlight(false);
	}

	if (UCGGameInstance* GameInstance = Cast<UCGGameInstance>(GetGameInstance()))
	{
		AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(GameInstance->GetCurrentPlayerController());
		if (PC && PC->GetCurrentInputMode() == EInputMode::UI)
		{
			PC->GoToNormalMode();
		}
	}

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
	if (bPendingPlacementEdit)
	{
		bPendingPlacementEdit = false;
		if (IsValid(CurrentWorkstation))
			if (UUIManagerSubsystem* UIManager = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
				if (UOfficeMainWidget* Main = UIManager->GetOfficeMain()) Main->RequestPlacementEdit(CurrentWorkstation);
	}
}

void UWorkstationInfoWidget::OnRelocateClicked()
{
	if (IsCloseRequested() || !IsValid(CurrentWorkstation)) return;
	bPendingPlacementEdit = true;
	CloseWithAnimation();
}

void UWorkstationInfoWidget::SetWorkstationData(AWorkstationActorBase* Workstation)
{
	CurrentWorkstation = Workstation;

	if (CurrentWorkstation)
	{
		CurrentWorkstation->SetHighlight(true);
		FocusCameraOnDesk();
	}

	UpdateUI();
}

void UWorkstationInfoWidget::SetEmployeeData(int32 EmployeeID)
{
	CurrentEmployeeID = EmployeeID;
	UpdateUI();
}

void UWorkstationInfoWidget::FocusCameraOnDesk()
{
	if (!CurrentWorkstation)
	{
		return;
	}

	if (AOfficeCameraPawn* CameraPawn = Cast<AOfficeCameraPawn>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0)))
	{
		CameraPawn->FocusOnLocation(CurrentWorkstation->GetActorLocation(), 600.f);
	}
}

void UWorkstationInfoWidget::SetOccupantHighlight(bool bHighlighted)
{
	if (CurrentEmployeeID < 0)
	{
		return;
	}

	if (AOfficeGameMode* OfficeGM = Cast<AOfficeGameMode>(GetWorld()->GetAuthGameMode()))
	{
		if (AOfficeworker* Worker = OfficeGM->FindEmployeeActorByID(CurrentEmployeeID))
		{
			Worker->SetSelected(bHighlighted);
		}
	}
}

void UWorkstationInfoWidget::SyncOccupancyFromWorkstation()
{
	if (!CurrentWorkstation)
	{
		return;
	}

	const int32 ChairCount = CurrentWorkstation->GetChairCount();

	// 칩 직원이 이 책상을 떠났으면 해제 (직원창 [자리 비우기]/해고 반영)
	if (CurrentEmployeeID >= 0)
	{
		bool bStillSeated = false;
		for (int32 SeatIdx = 0; SeatIdx < ChairCount; ++SeatIdx)
		{
			if (CurrentWorkstation->GetAssignedEmployeeID(SeatIdx) == CurrentEmployeeID)
			{
				bStillSeated = true;
				break;
			}
		}
		if (!bStillSeated)
		{
			SetOccupantHighlight(false);
			CurrentEmployeeID = -1;
		}
	}

	// 빈 칩 상태에서 자동 착석이 채웠으면 첫 점유자 채택
	if (CurrentEmployeeID < 0)
	{
		for (int32 SeatIdx = 0; SeatIdx < ChairCount; ++SeatIdx)
		{
			const int32 OccupantID = CurrentWorkstation->GetAssignedEmployeeID(SeatIdx);
			if (OccupantID >= 0)
			{
				CurrentEmployeeID = OccupantID;
				break;
			}
		}
	}
}

void UWorkstationInfoWidget::UpdateUI()
{
	if (!CurrentWorkstation)
	{
		return;
	}

	SyncOccupancyFromWorkstation();
	UpdateDeskSection();
	UpdateComponentSection();
	UpdateWorkerCard();
	UpdateBenchSection();
}

void UWorkstationInfoWidget::UpdateDeskSection()
{
	if (!CurrentWorkstation)
	{
		return;
	}

	// DT_WorkstationCard.DisplayName은 선택한 책상 이름이다. 비교 그림은 등급 장비 구성이다.
	FText DeskTitle;
	if (UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr)
	{
		bool bCardOK = false;
		const FWorkstationCardTable Card = TableMgr->GetWorkstationCardInfo(CurrentWorkstation->WorkstationTypeID, bCardOK);
		if (bCardOK) DeskTitle = Card.DisplayName;
	}

	const int32 SetupLevelAsInt = GetComputerSetupLevelNumber(CurrentWorkstation->GetCurrentSetupLevel());

	// 스택이 패널 인스턴스를 재사용하므로 이름이 없을 때도 덮는다 — 안 그러면 직전 책상 이름이 남는다
	if (PanelHeader)
	{
		PanelHeader->SetTitle(DeskTitle.IsEmpty()
			? NSLOCTEXT("WorkstationInfo", "DeskTitleFallback", "책상")
			: DeskTitle);
	}

	FComputerSetupLevelData CurrentLevelData;
	const bool bHasCurrent = CurrentWorkstation->GetCurrentSetupLevelData(CurrentLevelData);
	const bool bMax = CurrentWorkstation->IsMaxLevel();
	FComputerSetupLevelData TargetLevelData;
	const bool bHasTarget = bMax
		? CurrentWorkstation->GetMaxVariantData(TargetLevelData)
		: CurrentWorkstation->GetNextSetupLevelData(TargetLevelData);
	if (!bHasCurrent || (!bMax && !bHasTarget))
	{
		UE_LOG(LogTemp, Warning, TEXT("[WorkstationInfo] Missing setup row for current or next grade"));
		if (DeskCompareCard) DeskCompareCard->SetVisibility(ESlateVisibility::Collapsed);
		if (DeskPromotionCard) DeskPromotionCard->SetVisibility(ESlateVisibility::Collapsed);
		SetGradeCompositionImage(CurrentEquipImage, {});
		SetGradeCompositionImage(NextEquipImage, {});
		if (CurrentEquipText) CurrentEquipText->SetText(FText::GetEmpty());
		if (NextEquipText) NextEquipText->SetText(FText::GetEmpty());
		return;
	}

	if (DeskCompareCard) DeskCompareCard->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (DeskPromotionCard) DeskPromotionCard->SetVisibility(bHasTarget
		? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (NextCompareColumn) NextCompareColumn->SetVisibility(bHasTarget
		? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (CurrentGradeText) CurrentGradeText->SetText(FText::Format(
		NSLOCTEXT("WorkstationInfo", "CurrentComposition", "현재 등급 {0}"), SetupLevelAsInt));
	if (CurrentEquipText) CurrentEquipText->SetText(BuildWorkstationEquipSummary(CurrentLevelData));
	SetGradeCompositionImage(CurrentEquipImage, CurrentLevelData.PromotionImage);

	if (!bHasTarget)
	{
		if (NextGradeText) NextGradeText->SetText(FText::GetEmpty());
		if (NextEquipText) NextEquipText->SetText(FText::GetEmpty());
		SetGradeCompositionImage(NextEquipImage, {});
		if (RetainedEffectText) RetainedEffectText->SetText(NSLOCTEXT(
			"WorkstationInfo", "FinalGrade", "최대 등급입니다. 누적 효과는 유지됩니다."));
		return;
	}

	const int32 TargetGradeNumber = GetComputerSetupLevelNumber(TargetLevelData.Level);
	if (NextGradeText) NextGradeText->SetText(bMax
		? NSLOCTEXT("WorkstationInfo", "VariantComposition", "동급 장비 구성")
		: FText::Format(NSLOCTEXT("WorkstationInfo", "NextComposition", "다음 등급 {0}"), TargetGradeNumber));
	if (NextEquipText) NextEquipText->SetText(BuildWorkstationEquipSummary(TargetLevelData));
	SetGradeCompositionImage(NextEquipImage, TargetLevelData.PromotionImage);
	if (RetainedEffectText) RetainedEffectText->SetText(bMax
		? NSLOCTEXT("WorkstationInfo", "VariantRetained", "외형만 변경되며 성능과 누적 효과는 그대로입니다.")
		: NSLOCTEXT("WorkstationInfo", "PromotionRetained", "세 항목의 누적 효과는 다음 등급에도 유지됩니다."));

	const FText EquipmentChange = BuildPromotionResultLine(CurrentLevelData, TargetLevelData);
	const float SpeedDeltaPercentPoints = FMath::Max(0.f,
		(TargetLevelData.WorkSpeedBonusRate - CurrentLevelData.WorkSpeedBonusRate) * 100.f);
	if (PromotionTitleText) PromotionTitleText->SetText(bMax
		? NSLOCTEXT("WorkstationInfo", "VariantTitle", "동급 외형 변경")
		: FText::Format(NSLOCTEXT("WorkstationInfo", "PromotionTitle", "등급 {0}로 강화"), TargetGradeNumber));
	if (PromotionRewardText) PromotionRewardText->SetText(bMax
		? NSLOCTEXT("WorkstationInfo", "VariantReward", "외형만 변경 · 성능 동일")
		: FText::Format(NSLOCTEXT("WorkstationInfo", "PromotionReward", "{0} · 작업 속도 +{1}%p"),
			EquipmentChange, FText::AsNumber(SpeedDeltaPercentPoints)));
	const bool bReady = bMax || CurrentWorkstation->CanPromoteSetup();
	if (PromotionConditionText) PromotionConditionText->SetText(bMax
		? NSLOCTEXT("WorkstationInfo", "VariantCondition", "최대 등급 외형을 무료로 변경할 수 있습니다.")
		: bReady
			? NSLOCTEXT("WorkstationInfo", "PromotionReady", "강화 조건 완료")
			: NSLOCTEXT("WorkstationInfo", "PromotionCondition", "세 항목을 모두 최대 레벨로 강화하면 열립니다."));
	if (PromoteButton)
	{
		PromoteButton->SetMaxLevelState(false);
		PromoteButton->SetCost(0, EResourceType::Money, false);
		PromoteButton->SetCostVisible(true);
		PromoteButton->SetCostLabel(NSLOCTEXT("WorkstationInfo", "FreeCostLabel", "무료"));
		PromoteButton->SetButtonText(bMax
			? NSLOCTEXT("WorkstationInfo", "SwapAppearance", "변경")
			: NSLOCTEXT("WorkstationInfo", "PromoteDesk", "강화"));
		PromoteButton->SetLockedState(!bReady);
	}
}

void UWorkstationInfoWidget::UpdateComponentSection()
{
	UpdateComponentSlot(ProcessingUpgradeSlot, EWorkstationUpgradeType::Processing);
	UpdateComponentSlot(ToolsUpgradeSlot, EWorkstationUpgradeType::Tools);
	UpdateComponentSlot(EnvironmentUpgradeSlot, EWorkstationUpgradeType::Environment);
}

void UWorkstationInfoWidget::UpdateComponentSlot(UUpgradeSlot* UpgradeSlot, EWorkstationUpgradeType Type)
{
	if (!CurrentWorkstation || !UpgradeSlot) return;
	UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	const FWorkstationUpgradeDefinition* Definition = TableMgr ? TableMgr->FindWorkstationUpgradeDefinition(Type) : nullptr;
	if (!Definition)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WorkstationInfo] Missing upgrade definition for type %d"), static_cast<int32>(Type));
		UpgradeSlot->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	UpgradeSlot->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	UpgradeSlot->SetRuntimePresentation(Definition->DisplayName, Definition->Description, EResourceType::Money);
	UpgradeSlot->SetIconTexture(Definition->Icon.LoadSynchronous());
	UpgradeSlot->ValueUnit = TEXT("%");
	UpgradeSlot->ValuePrefix = Type == EWorkstationUpgradeType::Environment ? TEXT("") : TEXT("+");
	UpgradeSlot->bAbbreviateValue = Definition->EffectCurve == EWorkstationEffectCurve::Compound;

	const int32 RequestedCount = BulkModeSelector ? BulkModeSelector->GetRequestedCount() : 1;
	const bool bMaxAffordable = BulkModeSelector && BulkModeSelector->GetMode() == EEnhanceBulkMode::Max;
	FWorkstationUpgradeQuote Quote;
	if (!CurrentWorkstation->GetUpgradeQuote(Type, RequestedCount, bMaxAffordable, Quote) || !Quote.bValid)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WorkstationInfo] Invalid upgrade quote for type %d"), static_cast<int32>(Type));
		UpgradeSlot->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	const auto ToPercent = [Type](float RawBonus)
	{
		return Type == EWorkstationUpgradeType::Environment
			? 100.f * RawBonus / (1.f + RawBonus) : 100.f * RawBonus;
	};
	const int32 CurrentLevel = CurrentWorkstation->GetUpgradeLevel(Type);
	const bool bBulkMode = RequestedCount != 1 || bMaxAffordable;
	UpgradeSlot->SetLevelBadgeOverride(bBulkMode && Quote.Count > 0 && !Quote.bMaxed
		? FText::Format(NSLOCTEXT("WorkstationInfo", "BulkLevelPreview", "Lv.{0} → {1}"),
			CurrentLevel, CurrentLevel + Quote.Count)
		: FText::GetEmpty());
	UpgradeSlot->UpdateInfo(CurrentLevel,
		ToPercent(Quote.CurrentBonus), ToPercent(Quote.NextBonus), Quote.TotalCost,
		false, CurrentWorkstation->GetUpgradeMaxLevel(Type),
		bBulkMode, Quote.Count, Quote.TotalCost, Quote.Available);
	UpgradeSlot->SetSecondaryAnnotation(FText::GetEmpty());
}

void UWorkstationInfoWidget::HandleBulkModeChanged(EEnhanceBulkMode)
{
	UpdateComponentSection();
}

void UWorkstationInfoWidget::HandleResourceChanged(EResourceType Type, int64)
{
	if (Type == EResourceType::Money)
	{
		UpdateComponentSection();
	}
}

void UWorkstationInfoWidget::UpdateWorkerCard()
{
	if (!WorkerCard)
	{
		return;
	}

	FEmployeeInstance* Employee = (EmployeeManager && CurrentEmployeeID >= 0)
		? EmployeeManager->FindEmployee(CurrentEmployeeID)
		: nullptr;

	if (!Employee)
	{
		WorkerCard->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	// SelfHitTestInvisible — 카드 판은 히트 통과, 내부 [직원창] 버튼만 클릭
	WorkerCard->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	// ── 신원(초상·이름·Lv·★·종합·등급 젬) = 로스터 카드 한 부품이 통째로 담당 ──
	// 대기 목록·직원창과 같은 위젯이라 세 화면의 표시 규칙이 구조적으로 갈라질 수 없다.
	if (OccupantCard)
	{
		OccupantCard->SetEmployeeInfo(*Employee);
	}

	// ── 특화/주력/보조 (직원창 DisciplineCard 와 같은 직능 표시명 규칙) ──
	const int32 DiscCount = static_cast<int32>(EProductionDiscipline::Count);
	TArray<int32> Disc = Employee->DisciplinePoints;
	if (Disc.Num() != DiscCount) { Disc.SetNumZeroed(DiscCount); }

	int32 MaxVal = 0;
	for (int32 v : Disc) { MaxVal = FMath::Max(MaxVal, v); }

	// 표시명 SOT = DT_DisciplineDisplay (슬롯 의미는 enum 고정, 표시만 산업별 재해석)
	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* DiscTableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	UCGGameInstance* CGGI = Cast<UCGGameInstance>(GI);
	const ECompanyType Industry = CGGI ? CGGI->GetCurrentBuildingCompanyType() : ECompanyType::None;
	auto DiscName = [DiscTableMgr, Industry](int32 Idx) -> FText
	{
		return DiscTableMgr ? DiscTableMgr->GetDisciplineDisplayName(Industry, Idx) : FText::GetEmpty();
	};

	// 상위 2 직능 (동점 = 낮은 인덱스 우선)
	int32 Best0 = -1, Best1 = -1;
	for (int32 i = 0; i < DiscCount; ++i)
	{
		if (Best0 < 0 || Disc[i] > Disc[Best0]) { Best1 = Best0; Best0 = i; }
		else if (Best1 < 0 || Disc[i] > Disc[Best1]) { Best1 = i; }
	}
	if (OccupantSpecText)
	{
		OccupantSpecText->SetText(FText::FromString(MaxVal > 0
			? FString::Printf(TEXT("%s 특화"), *DiscName(Best0).ToString())
			: TEXT("특화 없음")));
	}
	if (OccupantLeadValue0) { OccupantLeadValue0->SetText(MaxVal > 0 ? DiscName(Best0) : FText::GetEmpty()); }
	if (OccupantLeadValue1) { OccupantLeadValue1->SetText(MaxVal > 0 ? DiscName(Best1) : FText::GetEmpty()); }

	// ── 연혁 ──
	if (OccupantNoText)
	{
		OccupantNoText->SetText(FText::FromString(FString::Printf(TEXT("#%04d"), Employee->EmployeeID)));
	}
	if (OccupantTenureText)
	{
		const FTimespan Span = FDateTime::Now() - Employee->HiredDate;
		const int32 TotalDays = FMath::Max(0, static_cast<int32>(Span.GetTotalDays()));
		const int32 Months = TotalDays / 30, Days = TotalDays % 30;
		OccupantTenureText->SetText(FText::FromString(
			Months > 0 ? FString::Printf(TEXT("%d개월 %d일"), Months, Days) : FString::Printf(TEXT("%d일"), Days)));
	}

	// 초상 로드는 OccupantCard 안에서 끝난다 — SetEmployeeInfo 가 자기 초상까지 채운다

	SetOccupantHighlight(true);
}

void UWorkstationInfoWidget::UpdateBenchSection()
{
	if (!CurrentWorkstation || !BenchSection) { return; }

	const bool bHasEmptySeat = CurrentWorkstation->FindEmptyAssignmentSlot() >= 0;
	BenchSection->SetVisibility(bHasEmptySeat ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);

	if (!bHasEmptySeat) { return; }

	UGameInstance* GI = GetGameInstance();
	UCGGameInstance* CGI = Cast<UCGGameInstance>(GI);
	UEmployeeManager* EmpMgr = GI ? GI->GetSubsystem<UEmployeeManager>() : nullptr;
	if (!CGI || !EmpMgr || !BenchListView) { return; }

	TArray<FEmployeeInstance> Bench = EmpMgr->GetUnassignedEmployeesInBuilding(CGI->GetCurrentManagedBuildingIndex());
	UOfficeManager::SortBenchByOverallDesc(Bench);

	BenchListView->ClearListItems();
	for (const FEmployeeInstance& Employee : Bench)
	{
		UEntityCardData* CardData = NewObject<UEntityCardData>(this);
		CardData->EmployeeInfo = Employee;
		BenchListView->AddItem(CardData);
	}

	if (BenchHeadText)
	{
		BenchHeadText->SetText(FText::Format(
			NSLOCTEXT("Office", "BenchHead", "대기 중인 직원 · {0}명"), FText::AsNumber(Bench.Num())));
	}

	const bool bEmpty = (Bench.Num() == 0);
	BenchListView->SetVisibility(bEmpty ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	// "누르면 이 책상에 앉습니다" 는 누를 직원이 있을 때만 맞는 말이다
	if (NoteText)
	{
		NoteText->SetVisibility(bEmpty ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
	if (BenchEmptyBox)
	{
		BenchEmptyBox->SetVisibility(bEmpty ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UWorkstationInfoWidget::OnBenchEntryGenerated(UUserWidget& EntryWidget)
{
	if (UEmployeeRosterCardWidget* Card = Cast<UEmployeeRosterCardWidget>(&EntryWidget))
	{
		// 재활용 엔트리라 매 생성마다 갈아끼운다 (누적 방지)
		Card->OnRosterCardClicked.RemoveDynamic(this, &UWorkstationInfoWidget::OnBenchCardClicked);
		Card->OnRosterCardClicked.AddDynamic(this, &UWorkstationInfoWidget::OnBenchCardClicked);
	}
}

void UWorkstationInfoWidget::OnBenchCardClicked(int32 EmployeeID)
{
	if (!CurrentWorkstation) { return; }
	if (UOfficeManager* OfficeMgr = GetWorld() ? GetWorld()->GetSubsystem<UOfficeManager>() : nullptr)
	{
		if (OfficeMgr->SeatEmployeeAtWorkstation(CurrentWorkstation, EmployeeID))
		{
			UpdateUI();
		}
	}
}

void UWorkstationInfoWidget::OnUnseatClicked()
{
	if (!CurrentWorkstation || CurrentEmployeeID < 0) { return; }
	if (UOfficeManager* OfficeMgr = GetWorld() ? GetWorld()->GetSubsystem<UOfficeManager>() : nullptr)
	{
		SetOccupantHighlight(false);
		if (OfficeMgr->UnseatEmployee(CurrentWorkstation, CurrentEmployeeID))
		{
			CurrentEmployeeID = -1;
			UpdateUI();
		}
	}
}

void UWorkstationInfoWidget::OnRecruitClicked()
{
	UCGGameInstance* CGI = Cast<UCGGameInstance>(GetGameInstance());
	UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	UUIManagerSubsystem* UIManager = GetGameInstance() ? GetGameInstance()->GetSubsystem<UUIManagerSubsystem>() : nullptr;
	if (!CGI || !TableMgr || !UIManager || !UIManager->GetUIBase()) { return; }

	TSubclassOf<UUserWidget> RecruitmentPanel = TableMgr->GetWidgetClass(EWidgetType::OfficeRecruitmentModal);
	if (!RecruitmentPanel) { return; }

	DeactivateWidget();
	UCommonActivatableWidget* Pushed = UIManager->GetUIBase()->PushPromptClass(RecruitmentPanel.Get());
	if (UOfficeRecruitmentModalWidget* RecruitmentWidget = Cast<UOfficeRecruitmentModalWidget>(Pushed))
	{
		RecruitmentWidget->SetBuildingIndex(CGI->GetCurrentManagedBuildingIndex());
		RecruitmentWidget->SetPreferredWorkstation(CurrentWorkstation);
	}
}

void UWorkstationInfoWidget::OnDeskUpgradeClicked()
{
	if (!IsValid(CurrentWorkstation))
	{
		return;
	}

	const bool bVariantChange = CurrentWorkstation->IsMaxLevel();
	FComputerSetupLevelData OldGrade;
	FComputerSetupLevelData NewGrade;
	if (!bVariantChange && (!CurrentWorkstation->GetCurrentSetupLevelData(OldGrade)
		|| !CurrentWorkstation->GetNextSetupLevelData(NewGrade)))
	{
		return;
	}
	const int32 OldGradeNumber = GetComputerSetupLevelNumber(CurrentWorkstation->GetCurrentSetupLevel());
	const bool bSucceeded = bVariantChange
		? CurrentWorkstation->TrySwapMaxVariant() : CurrentWorkstation->TryUpgradeSetupLevel();

	if (bSucceeded)
	{
		UpdateUI();
		if (bVariantChange)
		{
			return;
		}

		UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
		if (TableMgr)
		{
			bool bHasVFXRow = false;
			const FVFXTableRow VFXRow = TableMgr->GetVFXData(EVFXType::LevelUp, bHasVFXRow);
			if (bHasVFXRow)
			{
				if (UNiagaraSystem* System = TableMgr->GetVFXAsset(EVFXType::LevelUp))
				{
					UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), System,
						CurrentWorkstation->GetActorLocation() + VFXRow.LocationOffset,
						FRotator::ZeroRotator, FVector(VFXRow.DefaultScale), true);
				}
			}
		}
		if (USoundManagerSubsystem* SoundMgr = GetGameInstance()->GetSubsystem<USoundManagerSubsystem>())
		{
			SoundMgr->PlayUISound(CGUISoundTags::RewardCompanyLevelUp);
		}
		UUIManagerSubsystem* UIManager = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
		TSubclassOf<UUserWidget> PromotionClass = TableMgr
			? TableMgr->GetWidgetClass(EWidgetType::WorkstationPromotion) : nullptr;
		if (!PromotionClass || !UIManager || !UIManager->GetUIBase())
		{
			UE_LOG(LogTemp, Warning, TEXT("[WorkstationInfo] WorkstationPromotion is not registered"));
			return;
		}
		const int32 NewGradeNumber = GetComputerSetupLevelNumber(CurrentWorkstation->GetCurrentSetupLevel());
		const FText ResultCaption = BuildPromotionResultLine(OldGrade, NewGrade);
		UCommonActivatableWidget* PromotionWidget = UIManager->GetUIBase()->PushPromptClass(PromotionClass.Get(),
			[&](UCommonActivatableWidget& Widget)
			{
				if (UWorkstationPromotionWidget* Promotion = Cast<UWorkstationPromotionWidget>(&Widget))
				{
					Promotion->SetPromotionInfo(OldGradeNumber, NewGradeNumber,
						ResultCaption,
						(NewGrade.WorkSpeedBonusRate - OldGrade.WorkSpeedBonusRate) * 100.f,
						NewGrade.PromotionImage);
				}
			});
		if (PromotionWidget)
		{
			if (UCGGameInstance* GameInstance = Cast<UCGGameInstance>(GetGameInstance()))
			{
				if (AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(GameInstance->GetCurrentPlayerController()))
				{
					PC->GoToUIMode();
				}
			}
		}
	}
}

void UWorkstationInfoWidget::TryComponentUpgrade(EWorkstationUpgradeType Type, UUpgradeSlot* UpgradeSlot)
{
	if (!IsValid(CurrentWorkstation) || !UpgradeSlot) return;
	const int32 RequestedCount = BulkModeSelector ? BulkModeSelector->GetRequestedCount() : 1;
	const bool bMaxAffordable = BulkModeSelector && BulkModeSelector->GetMode() == EEnhanceBulkMode::Max;
	if (CurrentWorkstation->TryUpgradeComponent(Type, RequestedCount, bMaxAffordable))
	{
		UpgradeSlot->PlayUpgradeEffect();
		UpdateUI();
	}
}

UWidget* UWorkstationInfoWidget::GetFirstAffordableGrowthButton(bool bPromotionOnly) const
{
	if (!IsActivated() || !IsValid(CurrentWorkstation)) return nullptr;
	if (CurrentWorkstation->CanPromoteSetup())
	{
		return PromoteButton;
	}
	if (bPromotionOnly) return nullptr;
	const int32 RequestedCount = BulkModeSelector ? BulkModeSelector->GetRequestedCount() : 1;
	const bool bMaxAffordable = BulkModeSelector && BulkModeSelector->GetMode() == EEnhanceBulkMode::Max;
	for (const TPair<EWorkstationUpgradeType, UUpgradeSlot*> Candidate : {
		TPair<EWorkstationUpgradeType, UUpgradeSlot*>(EWorkstationUpgradeType::Processing, ProcessingUpgradeSlot),
		TPair<EWorkstationUpgradeType, UUpgradeSlot*>(EWorkstationUpgradeType::Tools, ToolsUpgradeSlot),
		TPair<EWorkstationUpgradeType, UUpgradeSlot*>(EWorkstationUpgradeType::Environment, EnvironmentUpgradeSlot) })
	{
		FWorkstationUpgradeQuote Quote;
		if (Candidate.Value && CurrentWorkstation->GetUpgradeQuote(Candidate.Key, RequestedCount, bMaxAffordable, Quote)
			&& Quote.bValid && Quote.Count > 0 && Quote.TotalCost <= Quote.Available)
		{
			return Candidate.Value->GetUpgradeButton();
		}
	}
	return nullptr;
}

UWidget* UWorkstationInfoWidget::GetCloseButtonWidget() const
{
	return PanelHeader ? PanelHeader->GetCloseButton() : nullptr;
}

void UWorkstationInfoWidget::OnProcessingUpgradeClicked()
{
	TryComponentUpgrade(EWorkstationUpgradeType::Processing, ProcessingUpgradeSlot);
}

void UWorkstationInfoWidget::OnToolsUpgradeClicked()
{
	TryComponentUpgrade(EWorkstationUpgradeType::Tools, ToolsUpgradeSlot);
}

void UWorkstationInfoWidget::OnEnvironmentUpgradeClicked()
{
	TryComponentUpgrade(EWorkstationUpgradeType::Environment, EnvironmentUpgradeSlot);
}

void UWorkstationInfoWidget::OnEmployeeWindowClicked()
{
	// 사무실 직원 패널 — 같은 BottomStack 이라 닫기 먼저, push 나중. 미등록이면 아래 직원창 경로로 폴백
	if (UOfficeEmployeePanelWidget::IsPanelClassRegistered(this))
	{
		const int32 PanelEmployeeID = CurrentEmployeeID;
		DeactivateWidget();
		UOfficeEmployeePanelWidget::PushForEmployee(this, PanelEmployeeID);
		return;
	}

	UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	UUIManagerSubsystem* UIManager = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
	if (!TableMgr || !UIManager || !UIManager->GetUIBase())
	{
		return;
	}

	TSubclassOf<UUserWidget> EmployeeWindow = TableMgr->GetWidgetClass(EWidgetType::EmployeeWindow);
	if (!EmployeeWindow)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WorkstationInfo] EmployeeWindow 클래스 미등록 — DT_WidgetClass 확인"));
		return;
	}

	if (auto* Window = Cast<UEmployeeWindowWidget>(UIManager->GetUIBase()->PushPromptClass(EmployeeWindow.Get())))
	{
		Window->OpenWithContext(CurrentEmployeeID, false);

		// 입력모드 쌍 — 닫힐 때 EmployeeWindow::NativeOnDeactivated 가 GoToNormalMode 복원 (도크는 아래 유지)
		if (UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance()))
		{
			if (AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(GI->GetCurrentPlayerController()))
			{
				PC->GoToUIMode();
			}
		}
	}
}

void UWorkstationInfoWidget::HandleEmployeeStatsChanged(int32 EmployeeID)
{
	if (EmployeeID != CurrentEmployeeID)
	{
		return;
	}

	// 강화(★)/초상 변화 → 칩 부분 갱신
	UpdateWorkerCard();
}

void UWorkstationInfoWidget::HandleRosterChanged()
{
	// 자동 착석/자리 비우기/해고가 이 책상 점유를 바꿨으면 칩 동기화 (직원창 아래 가려진 채로도 동작)
	const int32 PrevID = CurrentEmployeeID;
	SyncOccupancyFromWorkstation();
	if (CurrentEmployeeID != PrevID)
	{
		UpdateWorkerCard();
	}
}

void UWorkstationInfoWidget::OnCloseDelegate()
{
	CloseWithAnimation();
}

void UWorkstationInfoWidget::OnBackgroundClicked()
{
	CloseWithAnimation();
}
