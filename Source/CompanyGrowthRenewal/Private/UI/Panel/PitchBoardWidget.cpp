#include "UI/Panel/PitchBoardWidget.h"
#include "UI/UIBase.h"
#include "UI/Element/Common/ModalFrameWidget.h"
#include "Manager/PanelIntroSubsystem.h"
#include "TimerManager.h"

#include "Core/CGGameInstance.h"
#include "Data/GameSaveData.h"
#include "Data/ShippedRecord.h"
#include "Data/StageProgressData.h"
#include "Data/TierLayout.h"
#include "Enum/ItemType.h"
#include "Enum/ProjectDirection.h"
#include "Enum/QualityGrade.h"
#include "Enum/WidgetType.h"
#include "Manager/MissionManagerSubsystem.h"
#include "Manager/OfficeStageProgressManager.h"
#include "Manager/ManufacturingDevelopmentManager.h"
#include "Manager/ProductionOrderManager.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/UIManagerSubsystem.h"
#include "Table/ProjectGenreTable.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Cards/ItemCardWidget.h"
#include "UI/Element/Common/LaunchLootOddsWidget.h"
#include "UI/Element/Office/OfficeRequestCardWidget.h"
#include "UI/Element/Office/PitchTileWidget.h"
#include "UI/Element/Office/PitchTierTabWidget.h"

#include "CommonTextBlock.h"
#include "Groups/CommonButtonGroupBase.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/ScrollBox.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/Texture2D.h"
#include "GameMode/OfficeGameMode.h"
#include "Global/GlobalUtilFunctions.h"
#include "Manager/ManufacturingGuideCountryRules.h"
#include "Manager/OverseasActions.h"
#include "Manager/WorldFactoryManager.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	// ▶ 로 다음 티어를 미리 볼 수 있게 되는 문턱(현재 티어 클리어 수). 인라인 선착수 타일은 폐기.
	constexpr int32 PreviewThreshold = 4;

	// 카드 뒤 판정 글로우 색 — sRGB 토큰의 linear 변환(통과 #2F8A4C / 미달 #C4463E).
	// 알파 = "은은하게 퍼지는" 강도. 판정 없음(대기)은 알파 0 으로 꺼진다.
	const FLinearColor GlowPass(0.028f, 0.254f, 0.072f, 0.45f);
	const FLinearColor GlowFail(0.552f, 0.061f, 0.048f, 0.46f);
}

void UPitchBoardWidget::NativeConstruct()
{
	bIsBackHandler = true;
	Super::NativeConstruct();

	TierButtonGroup = NewObject<UCommonButtonGroupBase>(this);
	TierButtonGroup->SetSelectionRequired(true);
	if (UOfficeStageProgressManager* StageMgr = GetStageMgr())
	{
		CurrentTier = StageMgr->GetTierProgress().CurrentTier;
		if (ViewTier <= 0) { ViewTier = CurrentTier; }
	}
	RenderTierRail();
	TierButtonGroup->OnSelectedButtonBaseChanged.AddDynamic(this, &UPitchBoardWidget::HandleTierSelectionChanged);

	if (UCloseButtonWidget* PanelCloseButton = ModalFrame ? ModalFrame->GetCloseButton() : nullptr) { PanelCloseButton->OnCloseClicked.AddDynamic(this, &UPitchBoardWidget::OnCloseClicked); }
	if (BackgroundBtn) { BackgroundBtn->OnClicked.AddDynamic(this, &UPitchBoardWidget::OnBackgroundClicked); }
	if (RewardOddsButton) { RewardOddsButton->OnClicked().AddUObject(this, &UPitchBoardWidget::HandleRewardOddsClicked); }

	if (Card)
	{
		Card->OnCardSelected.AddDynamic(this, &UPitchBoardWidget::HandleCardAccepted);
	}
	if (ManufacturingMinusButton)
	{
		ManufacturingMinusButton->OnClicked().AddUObject(this, &UPitchBoardWidget::HandleManufacturingMinus);
		ManufacturingMinusButton->OnPressed().AddUObject(this, &UPitchBoardWidget::HandleManufacturingMinusPressed);
		ManufacturingMinusButton->OnReleased().AddUObject(this, &UPitchBoardWidget::HandleManufacturingStepReleased);
	}
	if (ManufacturingPlusButton)
	{
		ManufacturingPlusButton->OnClicked().AddUObject(this, &UPitchBoardWidget::HandleManufacturingPlus);
		ManufacturingPlusButton->OnPressed().AddUObject(this, &UPitchBoardWidget::HandleManufacturingPlusPressed);
		ManufacturingPlusButton->OnReleased().AddUObject(this, &UPitchBoardWidget::HandleManufacturingStepReleased);
	}
	if (MfgPresetQuarterButton) MfgPresetQuarterButton->OnClicked().AddUObject(this, &UPitchBoardWidget::HandleManufacturingQuarter);
	if (MfgPresetHalfButton) MfgPresetHalfButton->OnClicked().AddUObject(this, &UPitchBoardWidget::HandleManufacturingHalf);
	if (ManufacturingMaxButton) ManufacturingMaxButton->OnClicked().AddUObject(this, &UPitchBoardWidget::HandleManufacturingMax);
	if (ManufacturingReserveButton) ManufacturingReserveButton->OnClicked().AddUObject(this, &UPitchBoardWidget::HandleManufacturingReserve);
	if (MfgFactoryButton) MfgFactoryButton->OnClicked().AddUObject(this, &UPitchBoardWidget::HandleManufacturingFactoryMove);
	if (UManufacturingDevelopmentManager* PointsMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UManufacturingDevelopmentManager>() : nullptr)
	{
		PointsMgr->OnPointsChanged.AddUObject(this, &UPitchBoardWidget::HandleManufacturingPointsChanged);
	}

	// M6 미션 가이드 — 보드 열림 등록 (OpenBoard→PickInHouse 전이. 타일 지목은 하지 않는다)
	if (!IsManufacturingType(Cast<UCGGameInstance>(GetGameInstance()) ? Cast<UCGGameInstance>(GetGameInstance())->GetCurrentBuildingCompanyType() : ECompanyType::None))
	if (UMissionManagerSubsystem* MissionMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>() : nullptr)
	{
		MissionMgr->RegisterPitchBoard(this);
	}
}

void UPitchBoardWidget::NativeDestruct()
{
	if (TierButtonGroup)
	{
		TierButtonGroup->OnSelectedButtonBaseChanged.RemoveAll(this);
		TierButtonGroup->RemoveAll();
		TierButtonGroup = nullptr;
	}
	for (UPitchTierTabWidget* Tab : TierTabs)
	{
		if (Tab) { Tab->OnTierClicked.RemoveAll(this); }
	}
	TierTabs.Reset();
	bScrollSelectedTierPending = false;
	if (UCloseButtonWidget* PanelCloseButton = ModalFrame ? ModalFrame->GetCloseButton() : nullptr) { PanelCloseButton->OnCloseClicked.RemoveDynamic(this, &UPitchBoardWidget::OnCloseClicked); }
	if (BackgroundBtn) { BackgroundBtn->OnClicked.RemoveDynamic(this, &UPitchBoardWidget::OnBackgroundClicked); }
	if (RewardOddsButton) { RewardOddsButton->OnClicked().RemoveAll(this); }
	HideRewardOdds();
	if (RewardOddsPopup) { RewardOddsPopup->RemoveFromParent(); RewardOddsPopup = nullptr; }

	if (Card)
	{
		Card->OnCardSelected.RemoveDynamic(this, &UPitchBoardWidget::HandleCardAccepted);
	}
	if (ManufacturingMinusButton)
	{
		ManufacturingMinusButton->OnClicked().RemoveAll(this);
		ManufacturingMinusButton->OnPressed().RemoveAll(this);
		ManufacturingMinusButton->OnReleased().RemoveAll(this);
	}
	if (ManufacturingPlusButton)
	{
		ManufacturingPlusButton->OnClicked().RemoveAll(this);
		ManufacturingPlusButton->OnPressed().RemoveAll(this);
		ManufacturingPlusButton->OnReleased().RemoveAll(this);
	}
	if (MfgPresetQuarterButton) MfgPresetQuarterButton->OnClicked().RemoveAll(this);
	if (MfgPresetHalfButton) MfgPresetHalfButton->OnClicked().RemoveAll(this);
	if (ManufacturingMaxButton) ManufacturingMaxButton->OnClicked().RemoveAll(this);
	if (ManufacturingReserveButton) ManufacturingReserveButton->OnClicked().RemoveAll(this);
	if (MfgFactoryButton) MfgFactoryButton->OnClicked().RemoveAll(this);
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(ManufacturingRepeatTimer);
	if (UManufacturingDevelopmentManager* PointsMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UManufacturingDevelopmentManager>() : nullptr)
	{
		PointsMgr->OnPointsChanged.RemoveAll(this);
	}

	if (UMissionManagerSubsystem* MissionMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>() : nullptr)
	{
		MissionMgr->UnregisterPitchBoard(this);
	}

	Super::NativeDestruct();
}

void UPitchBoardWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	// 최초 진입 코치마크 — Rebuild 로 타일/티어가 채워진 뒤 딤을 덮는다
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(
			PanelIntroTimer, this, &UPitchBoardWidget::TryPlayPanelIntro, 0.5f, false);
	}

	// 열 때마다 현재 착석 로스터로 전부 재계산 — 저장하는 판정은 없다 (스펙 §6.2)
	DisplayedManufacturingCoverProject = INDEX_NONE;
	bManufacturingQuantityAtMax = true;
	Rebuild();
}

void UPitchBoardWidget::TryPlayPanelIntro()
{
	// 타이머 대기 중 창이 닫혔을 수 있다
	if (!IsActivated() || (PanelIntroOverlay.IsValid() && PanelIntroOverlay->IsInViewport()))
	{
		return;
	}
	UPanelIntroSubsystem* IntroMgr = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UPanelIntroSubsystem>() : nullptr;
	if (!IntroMgr)
	{
		return;
	}
	FName IntroKey(TEXT("PitchBoard"));
	if (IsManufacturingBoard())
	{
		const FBoardRow* SelectedRow = FindRow(SelectedIndex);
		if (!SelectedRow || SelectedRow->Verdict == PitchBoardText::ETileVerdict::Locked) { return; }
		IntroKey = SelectedRow->bDeveloped ? FName(TEXT("ManufacturingProduction")) : FName(TEXT("ManufacturingResearch"));
	}
	if (!IntroMgr->ShouldPlay(IntroKey)) { return; }
	if (bCardTransitioning)
	{
		GetWorld()->GetTimerManager().SetTimer(PanelIntroTimer, this, &UPitchBoardWidget::TryPlayPanelIntro, 0.5f, false);
		return;
	}
	PanelIntroOverlay = IntroMgr->PlayIntro(this, IntroKey, /*bIgnoreSeen=*/false);
	if (UPanelIntroOverlayWidget* Overlay = PanelIntroOverlay.Get())
	{
		Overlay->OnIntroFinished.AddUObject(this, &UPitchBoardWidget::HandlePanelIntroFinished);
	}
	else if (GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(PanelIntroTimer, this, &UPitchBoardWidget::TryPlayPanelIntro, 0.5f, false);
	}
}

void UPitchBoardWidget::HandlePanelIntroFinished()
{
	PanelIntroOverlay.Reset();
}

void UPitchBoardWidget::NativeOnDeactivated()
{
	HideRewardOdds();
	Super::NativeOnDeactivated();

	// 인트로는 뷰포트 오버레이라 스택이 안 거둬간다 — RemoveFromParent → NativeDestruct 가 복구 신호를 쏜다
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(PanelIntroTimer);
		GetWorld()->GetTimerManager().ClearTimer(ManufacturingRepeatTimer);
	}
	ManufacturingRepeatDirection = 0;
	if (UPanelIntroOverlayWidget* Intro = PanelIntroOverlay.Get())
	{
		Intro->RemoveFromParent();
	}
	PanelIntroOverlay.Reset();

	// CommonUI 는 인스턴스를 재사용한다 — 리셋하지 않으면 다시 열 때 지난 티어와 지난 선택이 그대로 뜬다
	ViewTier = 0;
	SelectedIndex = INDEX_NONE;
	bScrollSelectedTierPending = false;
}

bool UPitchBoardWidget::NativeOnHandleBackAction()
{
	if (!CGBackNav::IsTopWindow(*this))
	{
		return false;
	}
	if (RewardOddsPopup && RewardOddsPopup->IsVisible())
	{
		HideRewardOdds();
		return true;
	}
	return Super::NativeOnHandleBackAction();
}

UWidget* UPitchBoardWidget::GetRecommendCardCtaWidget() const
{
	return Card ? Card->GetPrimaryCtaWidget() : nullptr;
}

UWidget* UPitchBoardWidget::GetProjectTileWidget(int32 ProjectIndex) const
{
	for (UPitchTileWidget* Tile : Tiles)
	{
		if (Tile && Tile->GetProjectIndex() == ProjectIndex)
		{
			return Tile;
		}
	}
	return nullptr;
}

UWidget* UPitchBoardWidget::GetProjectCardCtaWidget(int32 ProjectIndex) const
{
	return SelectedIndex == ProjectIndex && !bCardTransitioning && Card
		? Card->GetPrimaryCtaWidget() : nullptr;
}

bool UPitchBoardWidget::IsProjectCardGateRisk(int32 ProjectIndex) const
{
	return GetProjectCardCtaWidget(ProjectIndex) && Card->IsPrimaryCtaGateRisk();
}

UWidget* UPitchBoardWidget::GetCloseButtonWidget() const
{
	return ModalFrame ? ModalFrame->GetCloseButton() : nullptr;
}

void UPitchBoardWidget::OpenTier(int32 Tier)
{
	UOfficeStageProgressManager* StageMgr = GetStageMgr();
	if (!StageMgr)
	{
		return;
	}

	const FProjectTierProgress& Progress = StageMgr->GetTierProgress();
	const int32 MaxOpenTier = (Progress.GetTierClearedCount(Progress.CurrentTier) >= PreviewThreshold)
		? FMath::Min(Progress.CurrentTier + 1, FTierLayout::MaxTier())
		: Progress.CurrentTier;

	const int32 NewTier = FMath::Clamp(Tier, 1, MaxOpenTier);
	if (NewTier == ViewTier)
	{
		return;
	}

	ViewTier = NewTier;
	// 티어를 옮기면 선택은 그 보드의 추천으로 다시 잡는다 — 앞 티어 번호가 남으면 카드가 빈 채로 뜬다
	SelectedIndex = INDEX_NONE;
	Rebuild();
}

void UPitchBoardWidget::OpenProject(int32 ProjectIndex)
{
	const UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance());
	UTableManagerSubsystem* TableMgr = GetTableMgr();
	if (!GI || !TableMgr || !IsManufacturingType(GI->GetCurrentBuildingCompanyType())) return;
	bool bFound = false;
	const FProjectData Product = TableMgr->GetProjectData(GI->GetCurrentBuildingCompanyType(), ProjectIndex, bFound);
	if (!bFound || Product.ProjectIndex != ProjectIndex) return;
	OpenTier(FTierLayout::GetTierForProject(ProjectIndex));
	const FBoardRow* ProductRow = FindRow(ProjectIndex);
	if (!ProductRow || ProductRow->Verdict == PitchBoardText::ETileVerdict::Locked) return;
	SelectedIndex = ProjectIndex;
	bManufacturingQuantityAtMax = true;
	UpdateTileSelection();
	RenderCard();
}

void UPitchBoardWidget::Rebuild()
{
	UOfficeStageProgressManager* StageMgr = GetStageMgr();
	UTableManagerSubsystem* TableMgr = GetTableMgr();
	if (!StageMgr || !TableMgr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PitchBoard] 매니저 미확보 - 보드 구성 중단"));
		return;
	}

	if (UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance()))
	{
		Industry = GI->GetCurrentBuildingCompanyType();
	}
	if (ModalFrame)
	{
		ModalFrame->SetTitle(IsManufacturingBoard()
			? NSLOCTEXT("PitchBoard", "ManufacturingTitle", "제품 선택")
			: NSLOCTEXT("PitchBoard", "PitchTitle", "기획 보드"));
	}
	if (TitleText)
	{
		TitleText->SetText(IsManufacturingBoard()
			? NSLOCTEXT("PitchBoard", "ManufacturingHeader", "제품 선택")
			: NSLOCTEXT("PitchBoard", "PitchHeader", "기획 보드"));
	}

	const FProjectTierProgress& Progress = StageMgr->GetTierProgress();
	CurrentTier = Progress.CurrentTier;
	const UMissionManagerSubsystem* MissionMgr = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>();
	const int32 RequiredProject = MissionMgr ? MissionMgr->GetRequiredProjectIndex() : 0;
	if (!IsManufacturingBoard() && RequiredProject > 0) ViewTier = FTierLayout::GetTierForProject(RequiredProject);
	if (ViewTier <= 0)
	{
		ViewTier = CurrentTier;
	}

	// 목록 전체가 같은 팀을 보므로 로스터 수집은 한 번만
	TArray<FEstimateWorkerInput> Roster;
	if (!IsManufacturingBoard()) StageMgr->BuildContributorRoster(Roster);

	TArray<int32> TeamPts;
	TeamPts.Init(0, 6);
	for (const FEstimateWorkerInput& Worker : Roster)
	{
		for (int32 SlotNo = 0; SlotNo < 6 && SlotNo < Worker.DisciplinePoints.Num(); ++SlotNo)
		{
			TeamPts[SlotNo] += Worker.DisciplinePoints[SlotNo];
		}
	}
	if (Card)
	{
		Card->SetTeamDisciplinePoints(TeamPts);
	}

	// 출시 기록 = 등급 글자 출처. 건물·산업·프로젝트로 찾는다.
	const TArray<FShippedProjectRecord>* Shipped = nullptr;
	int32 ManagedBuildingIndex = INDEX_NONE;
	if (UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance()))
	{
		ManagedBuildingIndex = GI->GetCurrentManagedBuildingIndex();
		if (USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>())
		{
			if (USaveGame_GameData* SaveData = SaveMgr->GetCurrentSaveData())
			{
				Shipped = &SaveData->GameData.ShippedProjects;
			}
		}
	}
	UManufacturingDevelopmentManager* ResearchMgr = IsManufacturingBoard()
		? GetGameInstance()->GetSubsystem<UManufacturingDevelopmentManager>() : nullptr;
	UProductionOrderManager* OrderMgr = IsManufacturingBoard()
		? GetGameInstance()->GetSubsystem<UProductionOrderManager>() : nullptr;
	// 제품별 확보 잔량 합 ― 같은 제품을 여러 번 확보하면 주문이 여러 장이라 더해서 보여 준다.
	TMap<int32, int64> SecuredByProject;
	if (OrderMgr && ManagedBuildingIndex >= 0)
	{
		for (const FProductionOrder& Order : OrderMgr->GetOrdersByBuilding(ManagedBuildingIndex))
		{
			if (Order.RemainingQuantity > 0) SecuredByProject.FindOrAdd(Order.ProjectIndex) += Order.RemainingQuantity;
		}
	}

	int32 ViewStart = 0, ViewEnd = -1;
	FTierLayout::GetRange(ViewTier, ViewStart, ViewEnd);

	const TArray<int32>& Developed = Progress.ClearedProjects;

	Rows.Reset();
	DisplayedResearchProjectIndex = INDEX_NONE;
	for (const FProjectData& Proj : TableMgr->GetProjectsByCompanyType(Industry))
	{
		if (!IsManufacturingBoard() && RequiredProject > 0 && Proj.ProjectIndex != RequiredProject) continue;
		// 현재(보고 있는) 티어의 프로젝트만 — 다음 티어 선착수 인라인 미리보기 폐기(사용자: "현재 티어 타일만")
		const bool bInView = (ViewEnd >= ViewStart) && (Proj.ProjectIndex >= ViewStart && Proj.ProjectIndex <= ViewEnd);
		if (!bInView)
		{
			continue;
		}
		if (!IsManufacturingBoard() && !Proj.IsPitchReady())
		{
			UE_LOG(LogTemp, Warning, TEXT("[PitchBoard] 미저작 행 #%d 건너뜀 - 가중치/장르 결손"), Proj.ProjectIndex);
			continue;
		}

		FBoardRow BoardRow;
		BoardRow.Project = Proj;
		BoardRow.bDeveloped = Developed.Contains(Proj.ProjectIndex);
		if (IsManufacturingBoard())
		{
			BoardRow.bHasShipped = BoardRow.bDeveloped;
			if (ResearchMgr && ManagedBuildingIndex >= 0)
			{
				const FManufacturingResearchStatus Research = ResearchMgr->GetResearchStatus(ManagedBuildingIndex, Proj.ProjectIndex);
				BoardRow.bResearchInProgress = Research.bInProgress;
				BoardRow.ResearchFraction = Research.RequiredWorkSeconds > 0.f
					? FMath::Clamp(Research.CompletedWorkSeconds / Research.RequiredWorkSeconds, 0.f, 1.f) : 0.f;
				DisplayedResearchProjectIndex = Research.ActiveProjectIndex;
			}
			const int64 Secured = SecuredByProject.FindRef(Proj.ProjectIndex);
			BoardRow.SecuredQuantity = static_cast<int32>(FMath::Min<int64>(Secured, MAX_int32));
			BoardRow.bQuantitySecured = Secured > 0;
			BoardRow.MaxReservable = OrderMgr && ManagedBuildingIndex >= 0 && BoardRow.bDeveloped
				? OrderMgr->GetMaxReservableQuantity(ManagedBuildingIndex, Proj.ProjectIndex) : 0;
			BoardRow.Verdict = FTierLayout::GetTierForProject(Proj.ProjectIndex) > CurrentTier
				? PitchBoardText::ETileVerdict::Locked : PitchBoardText::ETileVerdict::Pass;
		}

		FProjectBoardSlot& PitchSlot = BoardRow.BoardSlot;
		PitchSlot.bPitchCard = true;
		PitchSlot.ProjectIndex = Proj.ProjectIndex;
		PitchSlot.Genre = Proj.Genre;
		PitchSlot.Material = Proj.Material;
		PitchSlot.Cover = Proj.Icon;
		PitchSlot.Weight_Plan = Proj.Weight_Plan;
		PitchSlot.Weight_Dev = Proj.Weight_Dev;
		PitchSlot.Weight_Graphics = Proj.Weight_Graphics;
		PitchSlot.Weight_Sound = Proj.Weight_Sound;
		PitchSlot.Weight_Server = Proj.Weight_Server;
		PitchSlot.Weight_QA = Proj.Weight_QA;

		PitchSlot.ProjectName = !IsManufacturingBoard() && Proj.ProjectName.IsEmpty()
			? FText::Format(NSLOCTEXT("PitchBoard", "NameCompose", "{0} {1}"), FText::FromName(Proj.Material), FText::FromName(Proj.Genre))
			: Proj.ProjectName;

		PitchSlot.RequiredScore_Step1 = Proj.GetRequiredScore(1);
		PitchSlot.RequiredScore_Step2 = Proj.GetRequiredScore(2);
		PitchSlot.RequiredScore_Step3 = Proj.GetRequiredScore(3);
		PitchSlot.RequiredScore_Step4 = Proj.GetRequiredScore(4);
		if (IsManufacturingBoard())
		{
			FProjectGenreRow GenreRow;
			PitchSlot.GenreColor = TableMgr->GetGenreInfo(Industry, Proj.Genre, GenreRow)
				? GenreRow.Color : FLinearColor::Gray;
			Rows.Add(BoardRow);
			continue;
		}

		FProjectGenreRow GenreRow;
		PitchSlot.GenreColor = TableMgr->GetGenreInfo(Industry, Proj.Genre, GenreRow) ? GenreRow.Color : FLinearColor::Gray;

		// 판정·수익 단일 경로 — 출시 확인/결산서와 같은 추정기라 창구마다 다른 수치가 나오지 않는다
		const FProjectOutlookEstimate Outlook = StageMgr->EstimateProjectOutlook(Roster, Proj);
		PitchSlot.ExpectedQuality = Outlook.ExpectedQuality;
		PitchSlot.bGateRisk = Outlook.bGateRisk;
		PitchSlot.ExpectedDisciplineScores = Outlook.ExpectedDisciplineScores;
		PitchSlot.bHasOutlookEstimate = Outlook.bIsValid;
		PitchSlot.bHasUnmodeledEquipmentEffects = Outlook.bHasUnmodeledEquipmentEffects;

		int64 Revenue = 0;
		int32 OpSec = 0;
		StageMgr->EstimatePitchEconomy(Industry, PitchSlot.ExpectedQuality, Revenue, OpSec, Proj.ProjectIndex);
		PitchSlot.EstimatedRevenue = Revenue;
		PitchSlot.EstimatedOpTimeSec = OpSec;

		const FShippedProjectRecord* Record = Shipped && ManagedBuildingIndex >= 0
			? ShippedProjectRecords::Find(*Shipped, ManagedBuildingIndex, Industry, Proj.ProjectIndex)
			: nullptr;
		BoardRow.ReleaseGrade = Record ? Record->QualityGrade : EQualityGrade::Max;
		BoardRow.bHasShipped = Record != nullptr;

		float WorstRatio = 0.f;
		int32 WorstSlot = INDEX_NONE;
		ComputeWorstDiscipline(PitchSlot, WorstRatio, WorstSlot);

		BoardRow.Entry.ProjectIndex = Proj.ProjectIndex;
		BoardRow.Entry.bHasOutlook = PitchSlot.bHasOutlookEstimate;
		BoardRow.Entry.bGateRisk = PitchSlot.bGateRisk;
		BoardRow.Entry.bDeveloped = BoardRow.bDeveloped;
		BoardRow.Entry.EstimatedRevenue = PitchSlot.EstimatedRevenue;
		BoardRow.Entry.WorstRatio = WorstRatio;
		BoardRow.Entry.WorstSlot = WorstSlot;
		BoardRow.Entry.Grade = QualityGradeToAlphabetString(QualityScoreToGrade(PitchSlot.ExpectedQuality));

		// 보고 있는 티어가 진행 티어보다 위면(미도달) 그 보드는 통째로 잠금
		const int32 RowTier = FTierLayout::GetTierForProject(Proj.ProjectIndex);
		const bool bLockedRow = (RowTier > CurrentTier);
		BoardRow.Verdict = PitchBoardText::ClassifyTile(BoardRow.Entry, bLockedRow, false);

		Rows.Add(BoardRow);
	}

	// 번호순 고정 = 진행표. 정렬 토글을 두지 않는 이유(색이 이미 정렬 역할)라 여기서 한 번만 정한다
	Rows.Sort([](const FBoardRow& A, const FBoardRow& B) { return A.Project.ProjectIndex < B.Project.ProjectIndex; });

	TArray<PitchBoardText::FPitchBoardEntry> Entries;
	for (const FBoardRow& BoardRow : Rows)
	{
		if (BoardRow.Verdict == PitchBoardText::ETileVerdict::Locked || BoardRow.Verdict == PitchBoardText::ETileVerdict::Preview)
		{
			continue;
		}
		Entries.Add(BoardRow.Entry);
	}
	RecommendedIndex = IsManufacturingBoard() ? INDEX_NONE : PitchBoardText::ComputeRecommendedIndex(Entries);

	// 선택은 살아 있으면 유지(같은 보드를 다시 열어도 보던 기획안 그대로) — 없어졌으면 추천 → 가장 가까운 미달 → 첫 행
	const FBoardRow* KeptRow = FindRow(SelectedIndex);
	if (!KeptRow || KeptRow->Verdict == PitchBoardText::ETileVerdict::Locked)
	{
		// 새로 고른 제품은 최대 수량부터 보여 준다.
		bManufacturingQuantityAtMax = true;
		SelectedIndex = RecommendedIndex;
		if (SelectedIndex == INDEX_NONE && !IsManufacturingBoard())
		{
			SelectedIndex = PitchBoardText::ComputeClosestShortIndex(Entries);
		}
		if (SelectedIndex == INDEX_NONE)
		{
			// 잠금 행은 착수가 막히므로 열려 있는 첫 행을 고른다. 전부 잠금이면 표시만 첫 행으로 두고 착수는 HandleCardAccepted 가 거부한다
			const FBoardRow* FirstOpenRow = Rows.FindByPredicate([](const FBoardRow& Candidate)
				{ return Candidate.Verdict != PitchBoardText::ETileVerdict::Locked; });
			if (FirstOpenRow)
			{
				SelectedIndex = FirstOpenRow->Project.ProjectIndex;
			}
			else if (Rows.Num() > 0)
			{
				SelectedIndex = Rows[0].Project.ProjectIndex;
			}
		}
	}

	ApplyManufacturingLayout();
	RenderTierRail();
	RenderTiles();
	RenderCard();
	RenderGridHeader();
	if (IsManufacturingBoard())
	{
		if (RewardPreviewPanel) RewardPreviewPanel->SetVisibility(ESlateVisibility::Collapsed);
		if (RewardPreviewBox) RewardPreviewBox->SetVisibility(ESlateVisibility::Collapsed);
	}
	else
	{
		if (RewardPreviewPanel) RewardPreviewPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		if (RewardPreviewBox) RewardPreviewBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		RenderRewardPreview();
	}
}

void UPitchBoardWidget::RenderTiles()
{
	if (!TileGrid)
	{
		return;
	}

	TileGrid->ClearChildren();
	Tiles.Reset();

	UTableManagerSubsystem* TableMgr = GetTableMgr();
	// 제조업은 같은 C++ 의 제품 전용 변형 WBP ― 미등록이면 기본 타일로 떨어지고 경고를 남긴다.
	TSubclassOf<UUserWidget> TileClass = TableMgr && IsManufacturingBoard()
		? TableMgr->GetWidgetClass(EWidgetType::PitchTileProduct) : nullptr;
	if (IsManufacturingBoard() && !TileClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PitchBoard] PitchTileProduct 위젯 미등록(DT_WidgetClass) - 기본 타일로 표시"));
	}
	if (!TileClass && TableMgr)
	{
		TileClass = TableMgr->GetWidgetClass(EWidgetType::PitchTile);
	}
	if (!TileClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PitchBoard] PitchTile 위젯 미등록(DT_WidgetClass) - 타일 생성 실패"));
		return;
	}

	for (int32 Index = 0; Index < Rows.Num(); ++Index)
	{
		const FBoardRow& BoardRow = Rows[Index];
		UPitchTileWidget* Tile = CreateWidget<UPitchTileWidget>(this, TileClass);
		if (!Tile)
		{
			continue;
		}

		Tile->Configure(BuildTileData(BoardRow));
		// 매 렌더 새 인스턴스라 해제 쌍이 필요 없다 — 대신 같은 타일에 두 번 바인딩하지 않는다
		Tile->OnTileClicked.AddDynamic(this, &UPitchBoardWidget::HandleTileClicked);
		if (UUniformGridSlot* GridSlot = TileGrid->AddChildToUniformGrid(Tile, Index, 0))
		{
			GridSlot->SetHorizontalAlignment(HAlign_Fill);
			GridSlot->SetVerticalAlignment(VAlign_Fill);
		}

		Tiles.Add(Tile);
	}

	UpdateTileSelection();
}

FPitchTileData UPitchBoardWidget::BuildTileData(const FBoardRow& BoardRow) const
{
	FPitchTileData TileData;
	TileData.ProjectIndex = BoardRow.Project.ProjectIndex;
	TileData.ProjectName = BoardRow.BoardSlot.ProjectName;
	if (!BoardRow.Project.Genre.IsNone() && !BoardRow.Project.Material.IsNone())
	{
		TileData.Combo = FText::Format(NSLOCTEXT("PitchBoard", "Combo", "{0} · {1}"),
			FText::FromName(BoardRow.Project.Genre), FText::FromName(BoardRow.Project.Material));
	}
	else if (!BoardRow.Project.Genre.IsNone())
	{
		TileData.Combo = FText::FromName(BoardRow.Project.Genre);
	}
	else if (!BoardRow.Project.Material.IsNone())
	{
		TileData.Combo = FText::FromName(BoardRow.Project.Material);
	}
	TileData.Cover = BoardRow.Project.Icon;
	TileData.GenreColor = BoardRow.BoardSlot.GenreColor;
	TileData.Verdict = BoardRow.Verdict;
	TileData.CompanyType = Industry;
	TileData.ReleaseGrade = BoardRow.ReleaseGrade;
	TileData.bHasShipped = BoardRow.bHasShipped;
	TileData.bRecommended = !IsManufacturingBoard() && (BoardRow.Project.ProjectIndex == RecommendedIndex);
	TileData.bManufacturing = IsManufacturingBoard();
	TileData.bManufacturingResearchInProgress = BoardRow.bResearchInProgress;
	TileData.bManufacturingQuantitySecured = BoardRow.bQuantitySecured;
	TileData.ManufacturingSecuredQuantity = BoardRow.SecuredQuantity;
	TileData.ManufacturingMaxReservable = BoardRow.MaxReservable;
	TileData.ManufacturingResearchFraction = BoardRow.ResearchFraction;
	return TileData;
}

void UPitchBoardWidget::RefreshManufacturingTiles()
{
	const UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance());
	UProductionOrderManager* OrderMgr = GI ? GI->GetSubsystem<UProductionOrderManager>() : nullptr;
	UManufacturingDevelopmentManager* ResearchMgr = GI ? GI->GetSubsystem<UManufacturingDevelopmentManager>() : nullptr;
	const int32 BuildingID = GI ? GI->GetCurrentManagedBuildingIndex() : INDEX_NONE;
	if (!IsManufacturingBoard() || !OrderMgr || BuildingID < 0)
	{
		return;
	}
	for (FBoardRow& BoardRow : Rows)
	{
		BoardRow.MaxReservable = BoardRow.bDeveloped
			? OrderMgr->GetMaxReservableQuantity(BuildingID, BoardRow.Project.ProjectIndex) : 0;
		if (ResearchMgr && BoardRow.bResearchInProgress)
		{
			const FManufacturingResearchStatus Research = ResearchMgr->GetResearchStatus(BuildingID, BoardRow.Project.ProjectIndex);
			BoardRow.ResearchFraction = Research.RequiredWorkSeconds > 0.f
				? FMath::Clamp(Research.CompletedWorkSeconds / Research.RequiredWorkSeconds, 0.f, 1.f) : 0.f;
		}
	}
	for (UPitchTileWidget* Tile : Tiles)
	{
		if (const FBoardRow* BoardRow = Tile ? FindRow(Tile->GetProjectIndex()) : nullptr)
		{
			Tile->Configure(BuildTileData(*BoardRow));
		}
	}
	UpdateTileSelection();
}

void UPitchBoardWidget::UpdateTileSelection()
{
	// Configure 는 선택 링을 되돌리지 않는다 — 매 렌더 전 타일에 명시 세팅해야 이전 선택이 남지 않는다
	for (int32 Index = 0; Index < Tiles.Num(); ++Index)
	{
		if (UPitchTileWidget* Tile = Tiles[Index])
		{
			Tile->SetSelected(Tile->GetProjectIndex() == SelectedIndex);
		}
	}
}

namespace
{
	constexpr float CardExitDur  = 0.10f;   // 페이드아웃 + 아래로
	constexpr float CardEnterDur = 0.16f;   // 페이드인 + 위로
	constexpr float CardDropPx   = 42.0f;   // 빠지는 거리(가상px)
}

// 타일 선택이 바뀔 때 카드를 부드럽게 교체 — 나가는 페이즈 끝에서 RenderCard() 로 내용을 갈아끼운다.
void UPitchBoardWidget::BeginCardChangeTransition()
{
	if (!CardOverlay)   // 구 트리(오버레이 없음) = 즉시 렌더
	{
		RenderCard();
		return;
	}
	bCardTransitioning = true;
	bCardExiting = true;
	CardTransElapsed = 0.0f;
}

void UPitchBoardWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// ScrollBox와 새 자식의 Slate 위젯이 만들어진 다음 한 번만 노출을 요청한다.
	if (bScrollSelectedTierPending && TierRailScroll
		&& TierRailScroll->GetCachedGeometry().GetLocalSize().Y > 0.0f)
	{
		for (UPitchTierTabWidget* Tab : TierTabs)
		{
			if (Tab && Tab->GetTier() == ViewTier && Tab->GetCachedWidget().IsValid())
			{
				TierRailScroll->ScrollWidgetIntoView(Tab, false, EDescendantScrollDestination::IntoView, 8.0f);
				bScrollSelectedTierPending = false;
				break;
			}
		}
	}

	if (!bCardTransitioning || !CardOverlay)
	{
		return;
	}

	CardTransElapsed += InDeltaTime;

	if (bCardExiting)
	{
		const float A = FMath::Clamp(CardTransElapsed / CardExitDur, 0.0f, 1.0f);
		CardOverlay->SetRenderOpacity(1.0f - A);
		CardOverlay->SetRenderTranslation(FVector2D(0.0f, A * CardDropPx));
		if (A >= 1.0f)
		{
			RenderCard();               // 가려진 채 새 내용 교체
			bCardExiting = false;
			CardTransElapsed = 0.0f;
		}
	}
	else
	{
		const float A = FMath::Clamp(CardTransElapsed / CardEnterDur, 0.0f, 1.0f);
		const float E = 1.0f - (1.0f - A) * (1.0f - A);   // ease-out
		CardOverlay->SetRenderOpacity(E);
		CardOverlay->SetRenderTranslation(FVector2D(0.0f, (1.0f - E) * CardDropPx));   // 아래 → 0 (위로 올라옴)
		if (A >= 1.0f)
		{
			CardOverlay->SetRenderOpacity(1.0f);
			CardOverlay->SetRenderTranslation(FVector2D::ZeroVector);
			bCardTransitioning = false;
		}
	}
}

void UPitchBoardWidget::RenderCard()
{
	if (IsManufacturingBoard())
	{
		if (VerdictGlow) VerdictGlow->SetVisibility(ESlateVisibility::Collapsed);
		if (Card) Card->SetVisibility(ESlateVisibility::Collapsed);
		if (ManufacturingDetailRoot) ManufacturingDetailRoot->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		RenderManufacturingDetail();
		return;
	}
	if (Card) Card->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (VerdictGlow) VerdictGlow->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (ManufacturingDetailRoot) ManufacturingDetailRoot->SetVisibility(ESlateVisibility::Collapsed);
	if (!Card)
	{
		return;
	}

	const FBoardRow* BoardRow = FindRow(SelectedIndex);
	if (!BoardRow)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PitchBoard] 티어 %d 에 표시할 기획안이 없다 - 카드 갱신 생략"), ViewTier);
		return;
	}

	Card->SetReleaseGrade(Industry, BoardRow->ReleaseGrade);
	Card->SetSlotData(BoardRow->BoardSlot, 0, BoardRow->bHasShipped);
	Card->SetRecommended(false);

	// 카드 뒤 판정 글로우 — 표시 카드의 판정색으로 다크 셸에 번진다(카드의 통과/미달 신호와 동일 소스).
	// 통과(전망 있음·게이트 통과)=그린 / 미달(전망 있음·게이트 위험)=레드 / 대기(전망 없음)=투명.
	if (VerdictGlow)
	{
		FLinearColor GlowColor(0.f, 0.f, 0.f, 0.f);
		if (BoardRow->Entry.bHasOutlook)
		{
			GlowColor = BoardRow->Entry.bGateRisk ? GlowFail : GlowPass;
		}
		VerdictGlow->SetColorAndOpacity(GlowColor);
	}
}

namespace
{
	// 분야 행 상태 ― 막대 채움색과 아래 줄 잉크를 고른다.
	enum class EPitchMfgFieldSeverity : int32 { Normal = 0, Warn = 1, Short = 2 };

	FText PitchMfgWorkTime(float WorkSeconds)
	{
		const int32 TotalSeconds = FMath::Max(0, FMath::CeilToInt(FMath::Max(0.0f, WorkSeconds)));
		const int32 Minutes = TotalSeconds / 60;
		const int32 SecondsPart = TotalSeconds % 60;
		return Minutes > 0
			? FText::Format(NSLOCTEXT("PitchBoard", "ResearchMinutesSeconds", "{0}분 {1}초"),
				FText::AsNumber(Minutes), FText::AsNumber(SecondsPart))
			: FText::Format(NSLOCTEXT("PitchBoard", "ResearchSeconds", "{0}초"), FText::AsNumber(SecondsPart));
	}

	// 필요량·부족분은 올림, 남는 양은 내림 ― 「남음」이 실제로 쓸 수 있는 양보다 크게 보이지 않게 한다.
	FText PitchMfgFormatPoints(double Value, bool bRoundUp)
	{
		FNumberFormattingOptions Options;
		Options.SetMinimumFractionalDigits(0);
		Options.SetMaximumFractionalDigits(0);
		Options.SetRoundingMode(bRoundUp ? ERoundingMode::ToPositiveInfinity : ERoundingMode::ToNegativeInfinity);
		return FText::AsNumber(Value, &Options);
	}

	// 오피스 Strip 의 제조 3분야와 같은 아이콘 칸(설계=1, 공정기술=4, 품질검증=5).
	int32 PitchMfgFieldIconSlot(int32 FieldIndex)
	{
		return FieldIndex == 0 ? 1 : FieldIndex == 1 ? 4 : 5;
	}
}

void UPitchBoardWidget::CacheManufacturingFieldWidgets()
{
	if (bManufacturingFieldWidgetsCached)
	{
		return;
	}
	bManufacturingFieldWidgetsCached = true;
	for (int32 FieldIndex = 0; FieldIndex < 3; ++FieldIndex)
	{
		FManufacturingFieldWidgets& Widgets = ManufacturingFieldWidgets[FieldIndex];
		const auto Find = [this, FieldIndex](const TCHAR* Prefix)
		{
			return GetWidgetFromName(*FString::Printf(TEXT("%s%d"), Prefix, FieldIndex));
		};
		Widgets.Row = Find(TEXT("MfgFieldRow"));
		Widgets.Alert = Find(TEXT("MfgFieldAlert"));
		Widgets.Icon = Cast<UImage>(Find(TEXT("MfgFieldIcon")));
		Widgets.Name = Cast<UCommonTextBlock>(Find(TEXT("MfgFieldName")));
		Widgets.Bar = Cast<UProgressBar>(Find(TEXT("MfgFieldBar")));
		Widgets.Value = Cast<UCommonTextBlock>(Find(TEXT("MfgFieldValue")));
		Widgets.ValueSmall = Cast<UCommonTextBlock>(Find(TEXT("MfgFieldValueSmall")));
		Widgets.Sub = Cast<UCommonTextBlock>(Find(TEXT("MfgFieldSub")));
		Widgets.Check = Find(TEXT("MfgFieldCheck"));
		if (IsManufacturingBoard() && (!Widgets.Row.IsValid() || !Widgets.Name.IsValid() || !Widgets.Value.IsValid()))
		{
			UE_LOG(LogTemp, Warning, TEXT("[PitchBoard] 제조 분야 행 %d 위젯 누락 - UI_PitchBoardPanel 저작 확인"), FieldIndex);
		}
	}
}

void UPitchBoardWidget::ApplyManufacturingDetailDensity(bool bCompact)
{
	USizeBox* Hero = Cast<USizeBox>(GetWidgetFromName(TEXT("MfgHeroBox")));
	USizeBox* Cover = Cast<USizeBox>(GetWidgetFromName(TEXT("MfgHeroCoverBox")));
	USizeBox* FieldRows[3] = {};
	for (int32 Index = 0; Index < 3; ++Index)
	{
		FieldRows[Index] = Cast<USizeBox>(GetWidgetFromName(*FString::Printf(TEXT("MfgFieldRow%d"), Index)));
	}
	if (!Hero || !Cover || !FieldRows[0] || !FieldRows[1] || !FieldRows[2]) return;
	UVerticalBoxSlot* GapSlot = Cast<UVerticalBoxSlot>(FieldRows[1]->Slot);
	if (!OriginalDetailMetrics.IsSet())
	{
		OriginalDetailMetrics = FVector4f(Hero->GetHeightOverride(), Cover->GetHeightOverride(), FieldRows[0]->GetHeightOverride(),
			GapSlot ? GapSlot->GetPadding().Top : 0.f);
	}
	const FVector4f& Base = OriginalDetailMetrics.GetValue();
	auto Pick = [bCompact](float Compact, float Original) { return bCompact && Compact > 0.f ? Compact : Original; };
	Hero->SetHeightOverride(Pick(ManufacturingSecuredHeroHeight, Base.X));
	Cover->SetHeightOverride(Pick(ManufacturingSecuredCoverHeight, Base.Y));
	for (int32 Index = 0; Index < 3; ++Index)
	{
		FieldRows[Index]->SetHeightOverride(Pick(ManufacturingSecuredFieldRowHeight, Base.Z));
		if (UVerticalBoxSlot* RowSlot = Index > 0 ? Cast<UVerticalBoxSlot>(FieldRows[Index]->Slot) : nullptr)
		{
			FMargin Gap = RowSlot->GetPadding();
			Gap.Top = Pick(ManufacturingSecuredFieldRowGap, Base.W);
			RowSlot->SetPadding(Gap);
		}
	}
}

void UPitchBoardWidget::ApplyManufacturingLayout()
{
	if (USizeBox* HeaderBox = Cast<USizeBox>(GetWidgetFromName(TEXT("ProjectProgressHeight"))))
	{
		if (!OriginalListHeaderHeight.IsSet()) OriginalListHeaderHeight = HeaderBox->GetHeightOverride();
		HeaderBox->SetHeightOverride(IsManufacturingBoard() && ManufacturingListHeaderHeight > 0.f
			? ManufacturingListHeaderHeight : OriginalListHeaderHeight.GetValue());
	}
	const bool bInsets = IsManufacturingBoard() && bOverrideManufacturingInsets;
	if (UBorder* RailFrame = Cast<UBorder>(GetWidgetFromName(TEXT("RailBorder"))))
	{
		if (!OriginalRailPadding.IsSet()) OriginalRailPadding = RailFrame->GetPadding();
		RailFrame->SetPadding(bInsets ? ManufacturingRailPadding : OriginalRailPadding.GetValue());
	}
	if (UBorder* Tray = Cast<UBorder>(GetWidgetFromName(TEXT("ProjectSectionTray"))))
	{
		if (!OriginalTrayPadding.IsSet()) OriginalTrayPadding = Tray->GetPadding();
		Tray->SetPadding(bInsets ? ManufacturingTrayPadding : OriginalTrayPadding.GetValue());
	}
	USizeBox* RailBox = Cast<USizeBox>(GetWidgetFromName(TEXT("RailSizer")));
	USizeBox* ListBox = Cast<USizeBox>(GetWidgetFromName(TEXT("ProjectColumnWidth")));
	USizeBox* DetailBox = Cast<USizeBox>(GetWidgetFromName(TEXT("DetailColumnWidth")));
	if (!RailBox || !ListBox || !DetailBox)
	{
		if (IsManufacturingBoard() && (ManufacturingRailWidth > 0.f || ManufacturingListWidth > 0.f))
		{
			UE_LOG(LogTemp, Warning, TEXT("[PitchBoard] 제조 열 폭 대상(RailSizer/ProjectColumnWidth/DetailColumnWidth) 누락 - 원래 폭 유지"));
		}
		return;
	}
	if (!OriginalRailWidth.IsSet())
	{
		OriginalRailWidth = RailBox->GetWidthOverride();
		OriginalListWidth = ListBox->GetWidthOverride();
		OriginalDetailWidth = DetailBox->GetWidthOverride();
	}
	const float Rail = OriginalRailWidth.GetValue();
	const float List = OriginalListWidth.GetValue();
	const float Detail = OriginalDetailWidth.GetValue();
	const bool bResize = IsManufacturingBoard() && ManufacturingRailWidth > 0.f && ManufacturingListWidth > 0.f
		&& Rail > 0.f && List > 0.f && Detail > 0.f;
	// 세 열의 합을 그대로 두어 보드 바깥 크기가 변하지 않게 한다 ― 줄인 만큼 상세 열이 받는다.
	const float NewDetail = bResize ? Rail + List + Detail - ManufacturingRailWidth - ManufacturingListWidth : Detail;
	if (bResize && NewDetail <= 0.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PitchBoard] 제조 열 폭 합이 원래보다 큼 - 원래 폭 유지"));
		return;
	}
	RailBox->SetWidthOverride(bResize ? ManufacturingRailWidth : Rail);
	ListBox->SetWidthOverride(bResize ? ManufacturingListWidth : List);
	DetailBox->SetWidthOverride(NewDetail);
}

void UPitchBoardWidget::SetManufacturingFieldRow(int32 FieldIndex, bool bVisible, const FText& Value, const FText& ValueSmall,
	const FText& Sub, float BarFraction, int32 Severity, bool bShowCheck)
{
	if (FieldIndex < 0 || FieldIndex >= 3)
	{
		return;
	}
	FManufacturingFieldWidgets& Widgets = ManufacturingFieldWidgets[FieldIndex];
	if (UWidget* Row = Widgets.Row.Get())
	{
		Row->SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (!bVisible)
	{
		return;
	}
	const EPitchMfgFieldSeverity Level = static_cast<EPitchMfgFieldSeverity>(Severity);
	if (UWidget* Alert = Widgets.Alert.Get())
	{
		Alert->SetVisibility(Level == EPitchMfgFieldSeverity::Short ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
	if (UCommonTextBlock* ValueText = Widgets.Value.Get())
	{
		ValueText->SetText(Value);
	}
	if (UCommonTextBlock* SmallText = Widgets.ValueSmall.Get())
	{
		SmallText->SetText(ValueSmall);
		SmallText->SetVisibility(ValueSmall.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (UCommonTextBlock* SubText = Widgets.Sub.Get())
	{
		SubText->SetText(Sub);
		SubText->SetVisibility(Sub.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		SubText->SetColorAndOpacity(FSlateColor(Level == EPitchMfgFieldSeverity::Short ? ManufacturingSubShortInk
			: Level == EPitchMfgFieldSeverity::Warn ? ManufacturingSubWarnInk : ManufacturingSubInk));
	}
	if (UProgressBar* Bar = Widgets.Bar.Get())
	{
		Bar->SetPercent(FMath::Clamp(BarFraction, 0.f, 1.f));
		Bar->SetFillColorAndOpacity(Level == EPitchMfgFieldSeverity::Short ? ManufacturingBarShort
			: Level == EPitchMfgFieldSeverity::Warn ? ManufacturingBarWarn : ManufacturingBarNormal);
	}
	if (UWidget* Check = Widgets.Check.Get())
	{
		Check->SetVisibility(bShowCheck ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UPitchBoardWidget::SetManufacturingCta(const FText& Label, const FText& Sub, bool bEnabled)
{
	if (ManufacturingReserveButton)
	{
		ManufacturingReserveButton->SetIsEnabled(bEnabled);
		// 새 트리는 버튼 위에 얹은 두 줄 글자가 라벨을 맡는다 ― 버튼 자체 글자는 비운다.
		ManufacturingReserveButton->SetButtonText(MfgReserveLabel ? FText::GetEmpty() : Label);
	}
	if (MfgReserveLabel)
	{
		MfgReserveLabel->SetText(Label);
	}
	if (MfgReserveSub)
	{
		MfgReserveSub->SetText(Sub);
		MfgReserveSub->SetVisibility(Sub.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

void UPitchBoardWidget::RenderManufacturingDetail()
{
	if (GetWorld() && IsActivated() && !GetWorld()->GetTimerManager().IsTimerActive(PanelIntroTimer))
	{
		GetWorld()->GetTimerManager().SetTimer(PanelIntroTimer, this, &UPitchBoardWidget::TryPlayPanelIntro, 0.5f, false);
	}
	CacheManufacturingFieldWidgets();
	const FBoardRow* BoardRow = FindRow(SelectedIndex);
	UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance());
	UManufacturingDevelopmentManager* PointsMgr = GI ? GI->GetSubsystem<UManufacturingDevelopmentManager>() : nullptr;
	const int32 BuildingID = GI ? GI->GetCurrentManagedBuildingIndex() : INDEX_NONE;
	const bool bLocked = BoardRow && BoardRow->Verdict == PitchBoardText::ETileVerdict::Locked;
	const FManufacturingResearchStatus Research = BoardRow && PointsMgr && BuildingID >= 0
		? PointsMgr->GetResearchStatus(BuildingID, BoardRow->Project.ProjectIndex) : FManufacturingResearchStatus();
	ApplyManufacturingDetailDensity(Research.bResearched && BoardRow && BoardRow->bQuantitySecured);

	// 그림 무대 ― 이름 · 부제 · 단계
	if (ManufacturingProductName) ManufacturingProductName->SetText(BoardRow ? BoardRow->BoardSlot.ProjectName : FText::GetEmpty());
	if (MfgSubName)
	{
		const FText SubName = BoardRow ? BoardRow->Project.SubName : FText::GetEmpty();
		MfgSubName->SetText(SubName);
		MfgSubName->SetVisibility(SubName.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (MfgStepText)
	{
		MfgStepText->SetText(BoardRow
			? FText::Format(NSLOCTEXT("PitchBoard", "MfgStepText", "{0}단계 제품"), FTierLayout::GetTierForProject(BoardRow->Project.ProjectIndex))
			: FText::GetEmpty());
	}
	if (ManufacturingCoverImage && DisplayedManufacturingCoverProject != SelectedIndex)
	{
		DisplayedManufacturingCoverProject = SelectedIndex;
		UTexture2D* Cover = BoardRow && !BoardRow->Project.Icon.IsNull() ? BoardRow->Project.Icon.LoadSynchronous() : nullptr;
		if (Cover)
		{
			if (!ManufacturingCoverMID)
			{
				ManufacturingCoverMID = UPitchTileWidget::CreateProductCoverMID(this);
			}
			if (ManufacturingCoverMID)
			{
				ManufacturingCoverMID->SetTextureParameterValue(TEXT("Cover"), Cover);
				ManufacturingCoverImage->SetBrushFromMaterial(ManufacturingCoverMID);
			}
			else
			{
				ManufacturingCoverImage->SetBrushFromTexture(Cover);
			}
		}
		ManufacturingCoverImage->SetVisibility(Cover ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	// 분야 이름·아이콘은 두 상태가 같다 ― 표시명은 DT_DisciplineDisplay 만 쓴다.
	UTableManagerSubsystem* TableMgr = GetTableMgr();
	bool bLabelsValid = true;
	for (int32 FieldIndex = 0; FieldIndex < 3; ++FieldIndex)
	{
		const FText Name = TableMgr ? TableMgr->GetDisciplineDisplayName(Industry, FieldIndex) : FText::GetEmpty();
		if (Name.IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("[PitchBoard] 제조업 분야 %d 표시명 없음 — DT_DisciplineDisplay 확인"), FieldIndex);
			bLabelsValid = false;
		}
		FManufacturingFieldWidgets& Widgets = ManufacturingFieldWidgets[FieldIndex];
		if (UCommonTextBlock* NameText = Widgets.Name.Get())
		{
			NameText->SetText(Name);
		}
		if (UImage* Icon = Widgets.Icon.Get())
		{
			Icon->SetBrushFromTexture(UGlobalUtilFunctions::GetDisciplineIcon(PitchMfgFieldIconSlot(FieldIndex)), false);
			Icon->SetColorAndOpacity(UGlobalUtilFunctions::GetStepColor(FieldIndex + 1));
		}
	}

	if (!Research.bResearched)
	{
		RenderManufacturingResearch(Research, bLocked, bLabelsValid);
		return;
	}
	if (BoardRow)
	{
		RenderManufacturingQuantity(*BoardRow, bLocked, bLabelsValid);
	}
}

void UPitchBoardWidget::RenderManufacturingResearch(const FManufacturingResearchStatus& Research, bool bLocked, bool bLabelsValid)
{
	const bool bCapabilitiesValid = Research.bValid
		&& Research.TeamCapability.Num() == 3 && Research.RequiredCapability.Num() == 3;
	const float Fraction = Research.RequiredWorkSeconds > 0.0f
		? FMath::Clamp(Research.CompletedWorkSeconds / Research.RequiredWorkSeconds, 0.0f, 1.0f) : 0.0f;
	const int32 Percent = FMath::FloorToInt(100.0f * Fraction);
	const bool bOtherActive = !Research.bInProgress && Research.ActiveProjectIndex > 0;

	if (MfgStatusSwitcher) MfgStatusSwitcher->SetActiveWidgetIndex(Research.bInProgress ? 1 : 0);
	if (MfgStatusProgressBar) MfgStatusProgressBar->SetPercent(Fraction);
	if (MfgStatusProgressText)
	{
		MfgStatusProgressText->SetText(FText::Format(NSLOCTEXT("PitchBoard", "MfgPercent", "{0}%"), FText::AsNumber(Percent)));
	}
	if (MfgStatusNoneRight)
	{
		MfgStatusNoneRight->SetText(bLocked ? NSLOCTEXT("PitchBoard", "MfgNoneLocked", "단계 해금 후 개발")
			: bOtherActive ? NSLOCTEXT("PitchBoard", "MfgNoneOtherActive", "다른 제품 개발 중")
			: NSLOCTEXT("PitchBoard", "MfgNoneReady", "개발하면 수량 확보 가능"));
	}

	// 수량·부족·공장 이동은 개발을 마친 뒤에만 보인다.
	if (MfgQuantitySection) MfgQuantitySection->SetVisibility(ESlateVisibility::Collapsed);
	if (MfgShortageNotice) MfgShortageNotice->SetVisibility(ESlateVisibility::Collapsed);
	if (MfgFactoryRoot) MfgFactoryRoot->SetVisibility(ESlateVisibility::Collapsed);

	if (MfgPointsSection) MfgPointsSection->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (MfgPointsHeading) MfgPointsHeading->SetText(NSLOCTEXT("PitchBoard", "ResearchCapabilityHeading", "필요한 팀 능력"));
	if (MfgPointsHint) MfgPointsHint->SetText(NSLOCTEXT("PitchBoard", "ResearchCapabilityHint", "현재 / 필요"));
	for (int32 FieldIndex = 0; FieldIndex < 3; ++FieldIndex)
	{
		const int32 Need = bCapabilitiesValid ? Research.RequiredCapability[FieldIndex] : 0;
		if (!bCapabilitiesValid || Need <= 0)
		{
			SetManufacturingFieldRow(FieldIndex, false, FText::GetEmpty(), FText::GetEmpty(), FText::GetEmpty(), 0.f, 0, false);
			continue;
		}
		const int32 Team = Research.TeamCapability[FieldIndex];
		const bool bEnough = Team >= Need;
		SetManufacturingFieldRow(FieldIndex, true, FText::AsNumber(Team),
			FText::Format(NSLOCTEXT("PitchBoard", "ResearchCapabilityNeed", "/ {0}"), FText::AsNumber(Need)),
			FText::GetEmpty(), static_cast<float>(Team) / Need,
			static_cast<int32>(bEnough ? EPitchMfgFieldSeverity::Normal : EPitchMfgFieldSeverity::Short), bEnough);
	}

	if (MfgTimeSection) MfgTimeSection->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (Research.bInProgress)
	{
		if (MfgTimeHeading) MfgTimeHeading->SetText(NSLOCTEXT("PitchBoard", "ResearchProgressHeading", "개발 진행"));
		if (MfgTimeHint)
		{
			MfgTimeHint->SetText(FText::Format(NSLOCTEXT("PitchBoard", "ResearchRemaining", "남은 근무 {0}"),
				PitchMfgWorkTime(Research.RequiredWorkSeconds - Research.CompletedWorkSeconds)));
		}
		if (MfgTimeSwitcher) MfgTimeSwitcher->SetActiveWidgetIndex(1);
		if (MfgTimeProgressBar) MfgTimeProgressBar->SetPercent(Fraction);
		if (MfgTimePercent)
		{
			MfgTimePercent->SetText(FText::Format(NSLOCTEXT("PitchBoard", "MfgPercent", "{0}%"), FText::AsNumber(Percent)));
		}
		SetManufacturingCta(NSLOCTEXT("PitchBoard", "ResearchInProgressCta", "개발 진행 중"),
			NSLOCTEXT("PitchBoard", "ResearchWorking", "직원 근무에 따라 진행됩니다"), false);
		return;
	}

	if (MfgTimeHeading) MfgTimeHeading->SetText(NSLOCTEXT("PitchBoard", "ResearchTimeHeading", "개발 시간"));
	if (MfgTimeHint)
	{
		MfgTimeHint->SetText(Research.bValid ? FText::GetEmpty()
			: NSLOCTEXT("PitchBoard", "ResearchDataMissing", "제품 개발 정보 없음"));
	}
	if (MfgTimeSwitcher) MfgTimeSwitcher->SetActiveWidgetIndex(0);
	if (MfgTimeValue)
	{
		MfgTimeValue->SetText(FText::Format(NSLOCTEXT("PitchBoard", "ResearchDuration", "근무 {0}"),
			PitchMfgWorkTime(Research.RequiredWorkSeconds)));
	}
	const FText Reason = bLocked ? NSLOCTEXT("PitchBoard", "ResearchTierLocked", "단계 해금 후 개발할 수 있습니다")
		: !Research.BlockReason.IsEmpty() ? Research.BlockReason
		: bOtherActive ? NSLOCTEXT("PitchBoard", "OtherResearchActive", "다른 제품을 개발 중입니다")
		: Research.bCanStart ? NSLOCTEXT("PitchBoard", "ResearchStartReady", "개발을 마치면 수량을 확보할 수 있습니다")
		: NSLOCTEXT("PitchBoard", "ResearchStartUnavailable", "개발 조건을 확인해주세요");
	SetManufacturingCta(NSLOCTEXT("PitchBoard", "ResearchStartCta", "제품 개발 시작"), Reason,
		!bLocked && bCapabilitiesValid && bLabelsValid && Research.bCanStart);
}

void UPitchBoardWidget::RenderManufacturingQuantity(const FBoardRow& BoardRow, bool bLocked, bool bLabelsValid)
{
	UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance());
	UProductionOrderManager* OrderMgr = GI ? GI->GetSubsystem<UProductionOrderManager>() : nullptr;
	UManufacturingDevelopmentManager* PointsMgr = GI ? GI->GetSubsystem<UManufacturingDevelopmentManager>() : nullptr;
	UTableManagerSubsystem* TableMgr = GetTableMgr();
	const int32 BuildingID = GI ? GI->GetCurrentManagedBuildingIndex() : INDEX_NONE;
	const int32 ProjectIndex = BoardRow.Project.ProjectIndex;
	const TArray<double> Balances = PointsMgr && BuildingID >= 0 ? PointsMgr->GetDisciplinePoints(BuildingID) : TArray<double>();
	const TArray<double> UnitCosts = OrderMgr && BuildingID >= 0
		? OrderMgr->GetDevelopmentPointCosts(BuildingID, ProjectIndex) : TArray<double>();
	const int32 MaxQuantity = OrderMgr && BuildingID >= 0 ? OrderMgr->GetMaxReservableQuantity(BuildingID, ProjectIndex) : 0;
	if (bManufacturingQuantityAtMax)
	{
		ManufacturingQuantity = MaxQuantity;
	}
	ManufacturingQuantity = FMath::Clamp(ManufacturingQuantity, 1, FMath::Max(1, MaxQuantity));

	bool bCostReady = UnitCosts.Num() == 3 && Balances.Num() == 3 && bLabelsValid;
	bool bHasRequiredField = false;
	for (int32 FieldIndex = 0; bCostReady && FieldIndex < 3; ++FieldIndex)
	{
		const double Cost = UnitCosts[FieldIndex];
		bCostReady &= FMath::IsFinite(Cost) && Cost >= 0.0 && FMath::IsFinite(Balances[FieldIndex]);
		bHasRequiredField |= Cost > 0.0;
	}
	bCostReady &= bHasRequiredField;
	const bool bShort = bCostReady && MaxQuantity <= 0;
	const int32 Secured = BoardRow.SecuredQuantity;

	if (MfgStatusSwitcher) MfgStatusSwitcher->SetActiveWidgetIndex(Secured > 0 ? 4 : bShort ? 3 : 2);
	if (MfgStatusSecuredText)
	{
		MfgStatusSecuredText->SetText(FText::Format(NSLOCTEXT("PitchBoard", "MfgCount", "{0}개"), FText::AsNumber(Secured)));
	}
	if (MfgTimeSection) MfgTimeSection->SetVisibility(ESlateVisibility::Collapsed);
	if (MfgQuantitySection)
	{
		MfgQuantitySection->SetVisibility(bCostReady && !bLocked
			? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	// 부족하면 제목만 남기고 조절 줄 자리에 안내판을 보인다.
	for (const TCHAR* RowName : {TEXT("MfgStepRowBox"), TEXT("MfgPresetRowBox")})
	{
		if (UWidget* QuantityRow = GetWidgetFromName(RowName))
		{
			QuantityRow->SetVisibility(bShort ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
		}
	}
	if (MfgQuantityHint) MfgQuantityHint->SetVisibility(bShort ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	if (MfgShortageNotice)
	{
		MfgShortageNotice->SetVisibility(bShort && !bLocked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (MfgQuantityHeading)
	{
		MfgQuantityHeading->SetText(Secured > 0 ? NSLOCTEXT("PitchBoard", "MfgQuantityMore", "더 확보할 수량")
			: NSLOCTEXT("PitchBoard", "MfgQuantityHeading", "확보할 수량"));
	}
	if (MfgQuantityHint)
	{
		MfgQuantityHint->SetText(!bManufacturingQuantityAtMax
			? FText::Format(NSLOCTEXT("PitchBoard", "MfgQuantityMaxHint", "최대 {0}개"), FText::AsNumber(MaxQuantity))
			: Secured > 0 ? NSLOCTEXT("PitchBoard", "MfgQuantityAtMaxMore", "최대로 채워 둡니다")
			: NSLOCTEXT("PitchBoard", "MfgQuantityAtMax", "처음엔 최대로 채워 둡니다"));
	}
	if (ManufacturingQuantityText) ManufacturingQuantityText->SetText(FText::AsNumber(ManufacturingQuantity));
	if (ManufacturingMinusButton) ManufacturingMinusButton->SetIsEnabled(MaxQuantity > 0 && ManufacturingQuantity > 1);
	if (ManufacturingPlusButton) ManufacturingPlusButton->SetIsEnabled(ManufacturingQuantity < MaxQuantity);

	const int32 QuarterQuantity = FMath::Max(1, MaxQuantity / 4);
	const int32 HalfQuantity = FMath::Max(1, MaxQuantity / 2);
	const auto SetPreset = [MaxQuantity](UButtonWidget* Button, UCommonTextBlock* CountText, int32 Quantity)
	{
		if (Button) Button->SetIsEnabled(MaxQuantity > 0);
		if (CountText)
		{
			CountText->SetText(FText::Format(NSLOCTEXT("PitchBoard", "MfgCount", "{0}개"), FText::AsNumber(Quantity)));
		}
	};
	SetPreset(MfgPresetQuarterButton, MfgPresetQuarterCount, QuarterQuantity);
	SetPreset(MfgPresetHalfButton, MfgPresetHalfCount, HalfQuantity);
	SetPreset(ManufacturingMaxButton, MfgPresetMaxCount, MaxQuantity);
	// 값이 겹치면(최대가 아주 작을 때) 큰 쪽 버튼 하나만 링을 받는다.
	const bool bMaxRing = MaxQuantity > 0 && ManufacturingQuantity == MaxQuantity;
	const bool bHalfRing = !bMaxRing && ManufacturingQuantity == HalfQuantity;
	const bool bQuarterRing = !bMaxRing && !bHalfRing && ManufacturingQuantity == QuarterQuantity;
	if (MfgPresetMaxRing) MfgPresetMaxRing->SetVisibility(bMaxRing ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	if (MfgPresetHalfRing) MfgPresetHalfRing->SetVisibility(bHalfRing ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	if (MfgPresetQuarterRing) MfgPresetQuarterRing->SetVisibility(bQuarterRing ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);

	if (MfgPointsSection) MfgPointsSection->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (MfgPointsHeading) MfgPointsHeading->SetText(NSLOCTEXT("PitchBoard", "ManufacturingFieldHeading", "필요한 개발 포인트"));
	if (MfgPointsHint)
	{
		MfgPointsHint->SetText(bShort ? NSLOCTEXT("PitchBoard", "ManufacturingFieldPerUnit", "1개 기준")
			: NSLOCTEXT("PitchBoard", "ManufacturingFieldRemainHint", "확보하면 남는 양"));
	}
	// 1개분도 못 사면 1개 기준으로 무엇이 얼마나 모자란지 보여 준다.
	const double CalcQuantity = bShort ? 1.0 : static_cast<double>(ManufacturingQuantity);
	int32 ShortCount = 0;
	FText FirstShortName;
	double FirstDeficit = 0.0;
	for (int32 FieldIndex = 0; FieldIndex < 3; ++FieldIndex)
	{
		const double Cost = bCostReady ? UnitCosts[FieldIndex] : 0.0;
		if (!bCostReady || Cost <= 0.0)
		{
			SetManufacturingFieldRow(FieldIndex, false, FText::GetEmpty(), FText::GetEmpty(), FText::GetEmpty(), 0.f, 0, false);
			continue;
		}
		const double Balance = Balances[FieldIndex];
		const double Need = Cost * CalcQuantity;
		const double Remaining = Balance - Need;
		// 주황 = 이번 확보로 이 분야가 1개분도 안 남는다(= 지금 최대를 정하는 분야).
		const EPitchMfgFieldSeverity Level = Remaining < 0.0 ? EPitchMfgFieldSeverity::Short
			: Remaining < Cost ? EPitchMfgFieldSeverity::Warn : EPitchMfgFieldSeverity::Normal;
		const FText Sub = Remaining >= 0.0
			? FText::Format(NSLOCTEXT("PitchBoard", "ManufacturingFieldRemain", "남음 {0}"), PitchMfgFormatPoints(Remaining, false))
			: FText::Format(NSLOCTEXT("PitchBoard", "ManufacturingFieldShort", "부족 {0}"), PitchMfgFormatPoints(-Remaining, true));
		SetManufacturingFieldRow(FieldIndex, true, PitchMfgFormatPoints(Need, true), FText::GetEmpty(), Sub,
			Balance > 0.0 ? static_cast<float>(Need / Balance) : 1.f, static_cast<int32>(Level), false);
		if (Level == EPitchMfgFieldSeverity::Short)
		{
			if (ShortCount == 0)
			{
				FirstShortName = TableMgr ? TableMgr->GetDisciplineDisplayName(Industry, FieldIndex) : FText::GetEmpty();
				FirstDeficit = -Remaining;
			}
			++ShortCount;
		}
	}
	if (MfgShortageTitle)
	{
		MfgShortageTitle->SetText(ShortCount == 1
			? FText::Format(NSLOCTEXT("PitchBoard", "MfgShortageOne", "{0} 포인트가 {1} 모자랍니다"),
				FirstShortName, PitchMfgFormatPoints(FirstDeficit, true))
			: ShortCount > 1
				? FText::Format(NSLOCTEXT("PitchBoard", "MfgShortageMany", "{0} 외 {1}개 분야 포인트가 모자랍니다"),
					FirstShortName, FText::AsNumber(ShortCount - 1))
				: NSLOCTEXT("PitchBoard", "MfgShortageGeneric", "포인트가 모자랍니다"));
	}

	if (bLocked)
	{
		SetManufacturingCta(NSLOCTEXT("PitchBoard", "MfgReserveLockedCta", "수량 확보"),
			NSLOCTEXT("PitchBoard", "ManufacturingLocked", "잠금 해제 후 수량 확보 가능"), false);
	}
	else if (!bCostReady)
	{
		SetManufacturingCta(NSLOCTEXT("PitchBoard", "MfgReserveLockedCta", "수량 확보"),
			NSLOCTEXT("PitchBoard", "ManufacturingMissingCost", "제품 포인트 정보를 확인해주세요"), false);
	}
	else if (bShort)
	{
		// 공장 이동 버튼과 반씩 나누면 긴 안내가 넘친다 ― 그때는 짧은 안내.
		SetManufacturingCta(NSLOCTEXT("PitchBoard", "MfgShortCta", "포인트 부족"),
			Secured > 0 ? NSLOCTEXT("PitchBoard", "MfgShortCtaSubCompact", "포인트가 쌓이면 확보")
				: NSLOCTEXT("PitchBoard", "MfgShortCtaSub", "포인트가 쌓이면 다시 확보할 수 있습니다"), false);
	}
	else
	{
		SetManufacturingCta(Secured > 0
				? FText::Format(NSLOCTEXT("PitchBoard", "MfgReserveMore", "{0}개 더 확보"), FText::AsNumber(ManufacturingQuantity))
				: FText::Format(NSLOCTEXT("PitchBoard", "ManufacturingReserve", "{0}개 확보"), FText::AsNumber(ManufacturingQuantity)),
			Secured > 0 ? NSLOCTEXT("PitchBoard", "MfgReserveMoreSub", "지금 바로 확보")
				: NSLOCTEXT("PitchBoard", "ManufacturingReservationHint", "세계지도 공장에서 생산합니다"),
			true);
	}
	if (MfgFactoryRoot)
	{
		MfgFactoryRoot->SetVisibility(Secured > 0 && !bLocked ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}

int32 UPitchBoardWidget::GetManufacturingMaxQuantity() const
{
	const UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance());
	const UProductionOrderManager* OrderMgr = GI ? GI->GetSubsystem<UProductionOrderManager>() : nullptr;
	const FBoardRow* BoardRow = FindRow(SelectedIndex);
	return GI && OrderMgr && BoardRow
		? OrderMgr->GetMaxReservableQuantity(GI->GetCurrentManagedBuildingIndex(), BoardRow->Project.ProjectIndex) : 0;
}

void UPitchBoardWidget::SetManufacturingQuantity(int32 NewQuantity)
{
	if (!IsManufacturingBoard()) return;
	UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance());
	UManufacturingDevelopmentManager* ResearchMgr = GI ? GI->GetSubsystem<UManufacturingDevelopmentManager>() : nullptr;
	const FBoardRow* BoardRow = FindRow(SelectedIndex);
	if (!GI || !BoardRow || !ResearchMgr
		|| !ResearchMgr->GetResearchStatus(GI->GetCurrentManagedBuildingIndex(), BoardRow->Project.ProjectIndex).bResearched) return;
	const int32 MaxQuantity = GetManufacturingMaxQuantity();
	ManufacturingQuantity = FMath::Clamp(NewQuantity, 1, FMath::Max(1, MaxQuantity));
	bManufacturingQuantityAtMax = MaxQuantity > 0 && ManufacturingQuantity >= MaxQuantity;
	RenderManufacturingDetail();
}

void UPitchBoardWidget::HandleManufacturingMinus()
{
	if (bManufacturingRepeatFired)
	{
		bManufacturingRepeatFired = false;
		return;
	}
	SetManufacturingQuantity(ManufacturingQuantity - 1);
}

void UPitchBoardWidget::HandleManufacturingPlus()
{
	if (bManufacturingRepeatFired)
	{
		bManufacturingRepeatFired = false;
		return;
	}
	SetManufacturingQuantity(ManufacturingQuantity < MAX_int32 ? ManufacturingQuantity + 1 : MAX_int32);
}

void UPitchBoardWidget::HandleManufacturingQuarter() { SetManufacturingQuantity(FMath::Max(1, GetManufacturingMaxQuantity() / 4)); }
void UPitchBoardWidget::HandleManufacturingHalf() { SetManufacturingQuantity(FMath::Max(1, GetManufacturingMaxQuantity() / 2)); }
void UPitchBoardWidget::HandleManufacturingMax() { SetManufacturingQuantity(MAX_int32); }

void UPitchBoardWidget::HandleManufacturingMinusPressed()
{
	ManufacturingRepeatDirection = -1;
	ManufacturingRepeatTicks = 0;
	bManufacturingRepeatFired = false;
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(ManufacturingRepeatTimer, this,
			&UPitchBoardWidget::TickManufacturingRepeat, 0.07f, true, 0.35f);
	}
}

void UPitchBoardWidget::HandleManufacturingPlusPressed()
{
	ManufacturingRepeatDirection = 1;
	ManufacturingRepeatTicks = 0;
	bManufacturingRepeatFired = false;
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(ManufacturingRepeatTimer, this,
			&UPitchBoardWidget::TickManufacturingRepeat, 0.07f, true, 0.35f);
	}
}

void UPitchBoardWidget::HandleManufacturingStepReleased()
{
	ManufacturingRepeatDirection = 0;
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(ManufacturingRepeatTimer);
	}
}

void UPitchBoardWidget::TickManufacturingRepeat()
{
	if (ManufacturingRepeatDirection == 0 || !IsActivated())
	{
		HandleManufacturingStepReleased();
		return;
	}
	const int32 MaxQuantity = GetManufacturingMaxQuantity();
	// 처음엔 1씩, 오래 누르면 10씩, 더 누르면 최대의 2.5% 씩(최소 100) 움직인다.
	const int32 Step = ManufacturingRepeatTicks < 8 ? 1
		: ManufacturingRepeatTicks < 20 ? 10 : FMath::Max(100, MaxQuantity / 40);
	++ManufacturingRepeatTicks;
	const int32 Before = ManufacturingQuantity;
	const int64 Target = static_cast<int64>(ManufacturingQuantity) + static_cast<int64>(ManufacturingRepeatDirection) * Step;
	SetManufacturingQuantity(static_cast<int32>(FMath::Clamp<int64>(Target, 1, MAX_int32)));
	bManufacturingRepeatFired = true;
	if (ManufacturingQuantity == Before)
	{
		// 끝에 닿으면 버튼이 꺼져 손을 떼도 Released 가 안 올 수 있다 ― 여기서 멈춘다.
		HandleManufacturingStepReleased();
	}
}

ECountryType UPitchBoardWidget::FindManufacturingFactoryCountry(int32 ProjectIndex) const
{
	const UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GetTableMgr();
	const UWorldFactoryManager* Factories = GI ? GI->GetSubsystem<UWorldFactoryManager>() : nullptr;
	if (!TableMgr)
	{
		return ECountryType::None;
	}
	// 이 제품 라인이 있는 나라 → 같은 업종 라인이 있는 나라 → 이 업종 공장을 지을 수 있는 첫 나라.
	ECountryType FirstSupported = ECountryType::None;
	ECountryType FirstIndustryLine = ECountryType::None;
	for (const FCountryInfoTable& CountryRow : TableMgr->GetAllCountryInfos())
	{
		if (!ManufacturingGuideCountryRules::SupportsFactoryIndustry(CountryRow, Industry)
			|| !OverseasActions::IsCountryUnlocked(GI, CountryRow.CountryType))
		{
			continue;
		}
		if (FirstSupported == ECountryType::None) FirstSupported = CountryRow.CountryType;
		if (!Factories) continue;
		for (const FWorldFactoryLineState& Line : Factories->GetActiveLines(CountryRow.CountryType))
		{
			if (Line.CompanyType != Industry) continue;
			if (Line.ProductIndex == ProjectIndex) return CountryRow.CountryType;
			if (FirstIndustryLine == ECountryType::None) FirstIndustryLine = CountryRow.CountryType;
		}
	}
	return FirstIndustryLine != ECountryType::None ? FirstIndustryLine : FirstSupported;
}

void UPitchBoardWidget::HandleManufacturingFactoryMove()
{
	const FBoardRow* BoardRow = FindRow(SelectedIndex);
	UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance());
	if (!IsManufacturingBoard() || !BoardRow || !GI || GI->IsLevelTransitionActive())
	{
		return;
	}
	if (UUIManagerSubsystem* UIMgr = GI->GetSubsystem<UUIManagerSubsystem>())
	{
		UIMgr->RequestWorldMapOnCityReturn(FindManufacturingFactoryCountry(BoardRow->Project.ProjectIndex));
	}
	DeactivateWidget();
	// 오피스 정상 퇴장 경로(진행 저장 포함). 게임 모드가 없으면 같은 전환만 한다.
	if (AOfficeGameMode* OfficeMode = GetWorld() ? GetWorld()->GetAuthGameMode<AOfficeGameMode>() : nullptr)
	{
		OfficeMode->ReturnToMainMap();
	}
	else
	{
		GI->TransitionToLevel(TEXT("MainMap_City"));
	}
}

void UPitchBoardWidget::HandleManufacturingPointsChanged(int32 BuildingID)
{
	UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance());
	if (!IsManufacturingBoard() || !GI || BuildingID != GI->GetCurrentManagedBuildingIndex()) return;
	const FBoardRow* BoardRow = FindRow(SelectedIndex);
	UManufacturingDevelopmentManager* ResearchMgr = GI->GetSubsystem<UManufacturingDevelopmentManager>();
	if (BoardRow && ResearchMgr)
	{
		const FManufacturingResearchStatus Research = ResearchMgr->GetResearchStatus(BuildingID, SelectedIndex);
		const FBoardRow* ActiveRow = FindRow(DisplayedResearchProjectIndex);
		const FManufacturingResearchStatus ActiveResearch = ActiveRow
			? ResearchMgr->GetResearchStatus(BuildingID, DisplayedResearchProjectIndex) : FManufacturingResearchStatus();
		if (Research.bResearched != BoardRow->bDeveloped || Research.bInProgress != BoardRow->bResearchInProgress
			|| Research.ActiveProjectIndex != DisplayedResearchProjectIndex
			|| (ActiveRow && (ActiveResearch.bResearched != ActiveRow->bDeveloped
				|| ActiveResearch.bInProgress != ActiveRow->bResearchInProgress)))
		{
			// 개발 완료로 화면 역할이 바뀌면 이전 맥락의 설명도 함께 끝낸다.
			if (UPanelIntroOverlayWidget* Intro = PanelIntroOverlay.Get()) { Intro->RemoveFromParent(); }
			PanelIntroOverlay.Reset();
			Rebuild();
			return;
		}
	}
	RefreshManufacturingTiles();
	RenderManufacturingDetail();
}

void UPitchBoardWidget::HandleManufacturingReserve()
{
	if (!IsManufacturingBoard()) return;
	const FBoardRow* BoardRow = FindRow(SelectedIndex);
	UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance());
	UManufacturingDevelopmentManager* ResearchMgr = GI ? GI->GetSubsystem<UManufacturingDevelopmentManager>() : nullptr;
	UProductionOrderManager* OrderMgr = GI ? GI->GetSubsystem<UProductionOrderManager>() : nullptr;
	if (!BoardRow || !GI || !ResearchMgr || BoardRow->Verdict == PitchBoardText::ETileVerdict::Locked) return;
	const int32 BuildingID = GI->GetCurrentManagedBuildingIndex();
	const FManufacturingResearchStatus Research = ResearchMgr->GetResearchStatus(BuildingID, BoardRow->Project.ProjectIndex);
	if (!Research.bValid) return;
	UUIManagerSubsystem* UIMgr = GI->GetSubsystem<UUIManagerSubsystem>();
	if (!Research.bResearched)
	{
		FText Failure;
		if (!ResearchMgr->StartResearch(BuildingID, BoardRow->Project.ProjectIndex, Failure))
		{
			if (UIMgr && !Failure.IsEmpty()) UIMgr->ShowNotification(Failure, 3.0f, ENotificationType::Warning);
			RenderManufacturingDetail();
			return;
		}
		OnCloseClicked();
		return;
	}
	if (!OrderMgr) return;
	FText Failure;
	const int32 OrderID = OrderMgr->ReserveProductionQuantity(
		BuildingID, BoardRow->Project.ProjectIndex, ManufacturingQuantity, Failure);
	if (OrderID <= 0)
	{
		if (UIMgr && !Failure.IsEmpty()) UIMgr->ShowNotification(Failure, 3.0f, ENotificationType::Warning);
		RenderManufacturingDetail();
		return;
	}
	if (UIMgr) UIMgr->ShowNotification(NSLOCTEXT("PitchBoard", "ManufacturingReserved", "생산 가능 수량을 확보했습니다. 월드맵 공장에서 생산하세요."), 4.0f, ENotificationType::Success);
	Rebuild();
}

void UPitchBoardWidget::RenderTierRail()
{
	if (!TierRailBox || !TierButtonGroup)
	{
		return;
	}

	UOfficeStageProgressManager* StageMgr = GetStageMgr();
	UTableManagerSubsystem* TableMgr = GetTableMgr();
	if (!StageMgr || !TableMgr)
	{
		return;
	}

	// 제조업은 좁은 레일용 변형 탭 ― 미등록이면 기본 탭으로 떨어진다.
	TSubclassOf<UUserWidget> TabClass = IsManufacturingBoard()
		? TableMgr->GetWidgetClass(EWidgetType::PitchTierTabCompact) : nullptr;
	if (IsManufacturingBoard() && !TabClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PitchBoard] PitchTierTabCompact 위젯 미등록(DT_WidgetClass) - 기본 탭으로 표시"));
	}
	if (!TabClass)
	{
		TabClass = TableMgr->GetWidgetClass(EWidgetType::PitchTierTab);
	}
	if (!TabClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PitchBoard] PitchTierTab 위젯 미등록(DT_WidgetClass) - 티어 레일 생성 실패"));
		return;
	}

	const FProjectTierProgress& Progress = StageMgr->GetTierProgress();
	const int32 MaxTier = FTierLayout::MaxTier();
	const TGuardValue<bool> UpdatingGuard(bUpdatingTierRail, true);
	const bool bRecreateTabs = TierTabs.Num() != MaxTier
		|| (TierTabs.Num() > 0 && TierTabs[0] && TierTabs[0]->GetClass() != TabClass.Get());
	if (bRecreateTabs)
	{
		TierButtonGroup->RemoveAll();
		for (UPitchTierTabWidget* Tab : TierTabs)
		{
			if (Tab) { Tab->OnTierClicked.RemoveAll(this); }
		}
		TierRailBox->ClearChildren();
		TierTabs.Reset();
	}

	for (int32 Tier = 1; Tier <= MaxTier; ++Tier)
	{
		UPitchTierTabWidget* Tab = bRecreateTabs
			? CreateWidget<UPitchTierTabWidget>(this, TabClass) : TierTabs[Tier - 1].Get();
		if (!Tab)
		{
			continue;
		}

		FPitchTierTabData TabData;
		TabData.Tier = Tier;
		TabData.bLocked = (Tier > CurrentTier);
		TabData.bDone = (Tier < CurrentTier);
		TabData.bCurrent = (Tier == CurrentTier);
		int32 TierStart = 0, TierEnd = -1;
		FTierLayout::GetRange(Tier, TierStart, TierEnd);
		bool bFoundCoverProject = false;
		const FProjectData CoverProject = TableMgr->GetProjectData(Industry, TierStart, bFoundCoverProject);
		if (bFoundCoverProject)
		{
			TabData.Cover = CoverProject.Icon;
		}
		if (!bFoundCoverProject || TabData.Cover.IsNull())
		{
			UE_LOG(LogTemp, Warning, TEXT("[PitchBoard] Missing tier cover: industry=%d tier=%d project=%d"),
				static_cast<int32>(Industry), Tier, TierStart);
		}
		if (TabData.bCurrent)
		{
			const int32 Cleared = Progress.GetTierClearedCount(Tier);
			const int32 ProjectsThisTier = FTierLayout::ProjectsPerTier(Tier);
			// 제조 변형 탭은 좁은 이름 띠 오른쪽에 숫자만 둔다 ― 목록 머리와 같은 해금 기준(n/해금 개수)을 쓴다.
			TabData.ProgressText = IsManufacturingBoard()
				? FText::Format(NSLOCTEXT("PitchBoard", "TierTabResearchCount", "{0}/{1}"),
					FMath::Min(Cleared, FTierLayout::ClearToUnlock(Tier)), FTierLayout::ClearToUnlock(Tier))
				: FText::Format(NSLOCTEXT("PitchBoard", "TierTabReleaseCount", "{0}/{1} 출시"), Cleared, ProjectsThisTier);
			TabData.ProgressFraction = static_cast<float>(Cleared) / FMath::Max(1, ProjectsThisTier);
		}

		Tab->Configure(TabData);
		if (bRecreateTabs)
		{
			Tab->OnTierClicked.AddDynamic(this, &UPitchBoardWidget::HandleTierTabClicked);
			if (UPanelSlot* PanelSlot = TierRailBox->AddChild(Tab))
			{
				if (UVerticalBoxSlot* VBSlot = Cast<UVerticalBoxSlot>(PanelSlot))
				{
					VBSlot->SetHorizontalAlignment(HAlign_Fill);
					VBSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
				}
			}
			TierTabs.Add(Tab);
			TierButtonGroup->AddWidget(Tab);
		}
	}

	UpdateTierRailSelection();
}

void UPitchBoardWidget::UpdateTierRailSelection()
{
	for (UPitchTierTabWidget* Tab : TierTabs)
	{
		if (Tab && Tab->GetTier() == ViewTier && TierButtonGroup)
		{
			TierButtonGroup->SelectButtonAtIndex(TierButtonGroup->FindButtonIndex(Tab), false);
			bScrollSelectedTierPending = TierRailScroll != nullptr;
			break;
		}
	}
}

void UPitchBoardWidget::RenderGridHeader()
{
	UOfficeStageProgressManager* StageMgr = GetStageMgr();
	if (!StageMgr)
	{
		return;
	}

	const FProjectTierProgress& Progress = StageMgr->GetTierProgress();
	const int32 Cleared = Progress.GetTierClearedCount(ViewTier);
	const int32 Threshold = FTierLayout::ClearToUnlock(ViewTier);
	const bool bFinalTier = ViewTier >= FTierLayout::MaxTier();
	const bool bNextTierUnlocked = !bFinalTier && Progress.IsTierUnlocked(ViewTier + 1);

	// 제조 모드 = 「N단계 제품 · 다음 단계 눈금 · n / m」. 프로젝트 머리는 접는다.
	const bool bManufacturingHeader = IsManufacturingBoard() && MfgListHeader;
	if (MfgListHeader)
	{
		MfgListHeader->SetVisibility(bManufacturingHeader ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	// 프로젝트 머리 묶음(글 + 고정폭 막대 상자)째 접어야 제조 머리가 줄 전체 폭을 받는다.
	if (UWidget* ProjectHeaderWrap = Text_GridSub ? Text_GridSub->GetParent() : nullptr)
	{
		ProjectHeaderWrap->SetVisibility(bManufacturingHeader ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
	if (bManufacturingHeader)
	{
		if (MfgListTitle)
		{
			MfgListTitle->SetText(FText::Format(NSLOCTEXT("PitchBoard", "MfgListTitle", "{0}단계 제품"), ViewTier));
		}
		const bool bAllDone = bFinalTier || bNextTierUnlocked;
		if (MfgUnlockCaption)
		{
			MfgUnlockCaption->SetText(bFinalTier ? NSLOCTEXT("PitchBoard", "MfgUnlockAll", "모든 단계 해금")
				: bNextTierUnlocked ? NSLOCTEXT("PitchBoard", "MfgUnlockOpen", "다음 단계 열림")
				: NSLOCTEXT("PitchBoard", "MfgUnlockNext", "다음 단계"));
		}
		if (MfgUnlockCount)
		{
			MfgUnlockCount->SetText(FText::Format(NSLOCTEXT("PitchBoard", "MfgUnlockCount", "{0} / {1}"),
				FMath::Min(Cleared, Threshold), Threshold));
			MfgUnlockCount->SetVisibility(bAllDone ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		}
		// 눈금 칸 수 = 해금에 필요한 개수. 저작된 칸보다 많으면 그만큼만 보이고 경고한다.
		int32 AuthoredSegments = 0;
		for (int32 SegmentIndex = 0; SegmentIndex < 10; ++SegmentIndex)
		{
			UWidget* Segment = GetWidgetFromName(*FString::Printf(TEXT("MfgUnlockSeg%d"), SegmentIndex));
			UWidget* SegmentOn = GetWidgetFromName(*FString::Printf(TEXT("MfgUnlockSegOn%d"), SegmentIndex));
			if (!Segment) continue;
			++AuthoredSegments;
			const bool bShown = SegmentIndex < Threshold && !bAllDone;
			Segment->SetVisibility(bShown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			if (SegmentOn)
			{
				SegmentOn->SetVisibility(bShown && SegmentIndex < Cleared ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
			}
		}
		if (AuthoredSegments < Threshold)
		{
			UE_LOG(LogTemp, Warning, TEXT("[PitchBoard] 해금 눈금 칸 부족 - 필요 %d, 저작 %d"), Threshold, AuthoredSegments);
		}
		return;
	}
	if (Text_GridSub) Text_GridSub->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (HeaderProgressBar) HeaderProgressBar->SetVisibility(ESlateVisibility::HitTestInvisible);

	// 현재 열람 단계에서 다음 단계의 해금 상태만 간결하게 안내한다.
	if (Text_GridSub)
	{
		if (bFinalTier)
		{
			Text_GridSub->SetText(NSLOCTEXT("PitchBoard", "GridSubAllUnlocked", "모든 단계 해금"));
		}
		else if (bNextTierUnlocked)
		{
			Text_GridSub->SetText(NSLOCTEXT("PitchBoard", "GridSubNextUnlocked", "다음 단계 열림"));
		}
		else
		{
			Text_GridSub->SetText(FText::Format(
				NSLOCTEXT("PitchBoard", "GridSubNextUnlockProgress", "다음 단계 해금 {0} / {1}"), Cleared, Threshold));
		}
	}

	// 마지막 단계와 이미 열린 단계는 해금 진행을 완료 상태로 보여준다.
	if (HeaderProgressBar)
	{
		const float ProgressFraction = (bFinalTier || bNextTierUnlocked)
			? 1.0f
			: FMath::Clamp(static_cast<float>(Cleared) / FMath::Max(1, Threshold), 0.0f, 1.0f);
		HeaderProgressBar->SetPercent(ProgressFraction);
	}
}

void UPitchBoardWidget::RenderRewardPreview()
{
	HideRewardOdds();
	RewardPreviewEntries.Reset();
	if (!RewardPreviewBox) return;
	RewardPreviewBox->ClearChildren();
	if (Text_RewardTitle)
	{
		Text_RewardTitle->SetText(NSLOCTEXT("PitchBoard", "PossibleLaunchRewards", "출시 보상"));
	}
	UTableManagerSubsystem* TableMgr = GetTableMgr();
	ULaunchLootManagerSubsystem* LootMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<ULaunchLootManagerSubsystem>() : nullptr;
	if (LootMgr) RewardPreviewEntries = LootMgr->GetLootPreview(ViewTier);
	if (RewardPreviewPanel)
	{
		RewardPreviewPanel->SetVisibility(TableMgr && !RewardPreviewEntries.IsEmpty()
			? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (!TableMgr || RewardPreviewEntries.IsEmpty()) return;
	const TSubclassOf<UUserWidget> RewardClass = TableMgr->GetWidgetClass(EWidgetType::ItemCard);
	if (!RewardClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PitchBoard] ItemCard is not registered."));
		return;
	}
	for (const FLaunchLootPreviewEntry& Entry : RewardPreviewEntries)
	{
		UObject* RewardIcon = nullptr;
		FText RewardName;
		bool bFound = false;
		if (Entry.ItemType != EItemType::None)
		{
			RewardIcon = TableMgr->GetItemIconResource(Entry.ItemType, bFound);
			FShopItemTable ItemRow;
			if (TableMgr->GetShopItemByItemType(Entry.ItemType, ItemRow)) RewardName = ItemRow.DisplayName;
		}
		else if (Entry.ResourceType != EResourceType::None)
		{
			const FResourceInfo Info = TableMgr->GetResourceInfo(Entry.ResourceType, bFound);
			RewardName = Info.DisplayName;
			if (bFound && !Info.Icon.IsNull()) RewardIcon = Info.Icon.LoadSynchronous();
		}
		if (!RewardIcon || RewardName.IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("[PitchBoard] Missing reward icon/name: item=%d resource=%d"),
				static_cast<int32>(Entry.ItemType), static_cast<int32>(Entry.ResourceType));
		}
		UItemCardWidget* RewardCard = CreateWidget<UItemCardWidget>(this, RewardClass);
		if (!RewardCard) continue;
		if (UHorizontalBoxSlot* RewardSlot = RewardPreviewBox->AddChildToHorizontalBox(RewardCard))
		{
			RewardSlot->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
			RewardSlot->SetVerticalAlignment(VAlign_Center);
		}
		RewardCard->SetIconResource(RewardIcon);
		RewardCard->SetCardSize(RewardCardSize, RewardIconSize);
		RewardCard->SetToolTipText(RewardName);
		// 수량/확정 지급 툴팁은 켜지 않는다. 클릭은 같은 티어 전체 확률표 하나로 연결한다.
		RewardCard->OnItemCardClicked.BindWeakLambda(this, [this](FName)
		{
			HandleRewardOddsClicked();
		});
	}
}

void UPitchBoardWidget::HandleRewardOddsClicked()
{
	if (RewardPreviewEntries.IsEmpty() || !BackgroundBtn) return;
	if (!RewardOddsPopup)
	{
		RewardOddsPopup = CreateWidget<ULaunchLootOddsWidget>(this, ULaunchLootOddsWidget::StaticClass());
	}
	if (RewardOddsPopup)
	{
		RewardOddsPopup->Show(RewardPreviewEntries, BackgroundBtn->GetStyle());
	}
}

void UPitchBoardWidget::HideRewardOdds()
{
	if (RewardOddsPopup) RewardOddsPopup->Hide();
}

const UPitchBoardWidget::FBoardRow* UPitchBoardWidget::FindRow(int32 ProjectIndex) const
{
	if (ProjectIndex == INDEX_NONE)
	{
		return nullptr;
	}
	return Rows.FindByPredicate([ProjectIndex](const FBoardRow& BoardRow) { return BoardRow.Project.ProjectIndex == ProjectIndex; });
}

void UPitchBoardWidget::ComputeWorstDiscipline(const FProjectBoardSlot& BoardSlot, float& OutRatio, int32& OutSlot)
{
	OutRatio = 0.f;
	OutSlot = INDEX_NONE;

	const int32 W[6] = {
		BoardSlot.Weight_Plan, BoardSlot.Weight_Dev, BoardSlot.Weight_Graphics,
		BoardSlot.Weight_Sound, BoardSlot.Weight_Server, BoardSlot.Weight_QA
	};

	int32 MaxW = 0;
	for (int32 SlotNo = 0; SlotNo < 6; ++SlotNo)
	{
		MaxW = FMath::Max(MaxW, W[SlotNo]);
	}

	const float TargetBase = FStageProgressData::ComputeDisciplineTargetBase(
		BoardSlot.RequiredScore_Step1, BoardSlot.RequiredScore_Step2, BoardSlot.RequiredScore_Step3);

	if (!BoardSlot.bHasOutlookEstimate)
	{
		return;
	}

	TArray<float> Targets; Targets.Reserve(6);
	TArray<float> Scores;  Scores.Reserve(6);
	for (int32 SlotNo = 0; SlotNo < 6; ++SlotNo)
	{
		Targets.Add(FStageProgressData::ComputeDisciplineTarget(W[SlotNo], MaxW, TargetBase));
		Scores.Add(BoardSlot.ExpectedDisciplineScores.IsValidIndex(SlotNo)
			? BoardSlot.ExpectedDisciplineScores[SlotNo] : 0.f);
	}

	PitchBoardText::FindWorstDiscipline(Scores, Targets, OutSlot, OutRatio);
}

void UPitchBoardWidget::HandleTileClicked(int32 ProjectIndex)
{
	const FBoardRow* BoardRow = FindRow(ProjectIndex);
	if (!BoardRow || BoardRow->Verdict == PitchBoardText::ETileVerdict::Locked)
	{
		return;
	}

	SelectedIndex = ProjectIndex;
	if (IsManufacturingBoard()) bManufacturingQuantityAtMax = true;
	UpdateTileSelection();
	BeginCardChangeTransition();
}

void UPitchBoardWidget::HandleCardAccepted(int32 SlotIndex)
{
	// 보드의 카드는 1장(슬롯 0)뿐 — 착수 대상은 슬롯 번호가 아니라 지금 선택된 타일이다
	UE_LOG(LogTemp, Verbose, TEXT("[PitchBoard] 카드 CTA - 슬롯 %d / 선택 #%d"), SlotIndex, SelectedIndex);

	const FBoardRow* BoardRow = FindRow(SelectedIndex);
	UOfficeStageProgressManager* StageMgr = GetStageMgr();
	if (!BoardRow || !StageMgr)
	{
		return;
	}
	if (BoardRow->Verdict == PitchBoardText::ETileVerdict::Locked)
	{
		// 카드에 잠금 행이 올라오는 경우는 보드 전체가 잠긴 티어뿐 — 매니저엔 티어 게이트가 없어 여기서 막는다
		UE_LOG(LogTemp, Warning, TEXT("[PitchBoard] 잠긴 기획안 #%d 착수 거부 - 티어 미해금"), BoardRow->Project.ProjectIndex);
		return;
	}

	// 카드가 보여준 실제 행(이름/커버/요구점수)으로 착수 — 규모 슬롯 재추첨 없이 그 idx 행 그대로
	const EDevelopStartResult Result = StageMgr->StartSelfDevelopFromProject(
		BoardRow->Project.ProjectIndex, EProjectDirection::Standard, BoardRow->BoardSlot.ProjectName);

	// 실패 사유를 알리고 보드는 열어 둔다 — 타일이 그대로 남아야 다른 기획안으로 옮겨 갈 수 있다
	if (Result != EDevelopStartResult::Success)
	{
		UOfficeStageProgressManager::NotifyDevelopStartFailure(Result);
		return;
	}

	// M6 미션 가이드 — 기획안 선택 완료 → [출시] 단계로 전이
	if (UMissionManagerSubsystem* MissionMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>() : nullptr)
	{
		MissionMgr->NotifyInHouseProjectSelected();
	}
	DeactivateWidget();
}

void UPitchBoardWidget::HandleTierTabClicked(int32 Tier)
{
	// 잠긴 티어(현재보다 높은)를 누르면 이동 대신 안내 Toast — 프로젝트 공용 알림 채널 재사용
	if (Tier > CurrentTier)
	{
		if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
		{
			UIMgr->ShowNotification(
				FText::Format(NSLOCTEXT("PitchBoard", "TierLockedToast", "{0}단계 · 아직 잠겨 있습니다. 현재 단계를 먼저 완료하세요."), Tier),
				2.5f, ENotificationType::Warning);
		}
		return;
	}
}

void UPitchBoardWidget::HandleTierSelectionChanged(UCommonButtonBase* SelectedButton, int32 ButtonIndex)
{
	if (bUpdatingTierRail)
	{
		return;
	}
	if (UPitchTierTabWidget* Tab = Cast<UPitchTierTabWidget>(SelectedButton))
	{
		if (Tab->GetTier() <= CurrentTier)
		{
			OpenTier(Tab->GetTier());
		}
	}
}

void UPitchBoardWidget::OnCloseClicked()
{
	DeactivateWidget();
}

void UPitchBoardWidget::OnBackgroundClicked()
{
	DeactivateWidget();
}

UTableManagerSubsystem* UPitchBoardWidget::GetTableMgr() const
{
	return GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
}

UOfficeStageProgressManager* UPitchBoardWidget::GetStageMgr() const
{
	return GetWorld() ? GetWorld()->GetSubsystem<UOfficeStageProgressManager>() : nullptr;
}
