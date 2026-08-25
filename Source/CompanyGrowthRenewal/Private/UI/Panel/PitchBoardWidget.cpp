#include "UI/Panel/PitchBoardWidget.h"

#include "Core/CGGameInstance.h"
#include "Data/GameSaveData.h"
#include "Data/ShippedRecord.h"
#include "Data/StageProgressData.h"
#include "Data/TierLayout.h"
#include "Enum/ItemType.h"
#include "Enum/ProjectDirection.h"
#include "Enum/QualityGrade.h"
#include "Enum/WidgetType.h"
#include "Manager/ItemInventoryManager.h"
#include "Manager/MissionManagerSubsystem.h"
#include "Manager/OfficeStageProgressManager.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/TrendManagerSubsystem.h"
#include "Manager/UIManagerSubsystem.h"
#include "Player/MainMapPlayerController.h"
#include "Table/ProjectGenreTable.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "UI/Element/Office/OfficeRequestCardWidget.h"
#include "UI/Element/Office/PitchTileWidget.h"
#include "UI/UIBase.h"

#include "CommonTextBlock.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/UniformGridPanel.h"

namespace
{
	// 다음 티어 미리보기 개시 문턱 (스펙 §4.2)
	constexpr int32 PreviewThreshold = 4;

	// 그리드 = 4열 × 3행. 11·12칸이 다음 티어 미리보기 자리라 열 수와 미리보기 수가 같이 묶여 있다
	constexpr int32 TileColumns = 4;
	constexpr int32 PreviewTileCount = 2;

	// 키커 3색 — 카드 밴드/타일과 같은 팔레트 (linear)
	const FLinearColor KickerGold(0.76f, 0.40f, 0.04f, 1.f);
	const FLinearColor KickerInk2(0.49f, 0.55f, 0.62f, 1.f);
	const FLinearColor KickerAlert(0.96f, 0.64f, 0.61f, 1.f);

	const FLinearColor MeterOn(0.22f, 0.58f, 0.40f, 1.f);
	const FLinearColor MeterOff(0.84f, 0.86f, 0.87f, 0.12f);
}

void UPitchBoardWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (PrevTierButton) { PrevTierButton->OnClicked().AddUObject(this, &UPitchBoardWidget::HandlePrevTier); }
	if (NextTierButton) { NextTierButton->OnClicked().AddUObject(this, &UPitchBoardWidget::HandleNextTier); }
	if (TrendRerollButton) { TrendRerollButton->OnClicked().AddUObject(this, &UPitchBoardWidget::HandleTrendReroll); }
	if (BackToRecommendButton) { BackToRecommendButton->OnClicked().AddUObject(this, &UPitchBoardWidget::HandleBackToRecommend); }
	if (CloseButton) { CloseButton->OnCloseClicked.AddDynamic(this, &UPitchBoardWidget::OnCloseClicked); }
	if (BackgroundBtn) { BackgroundBtn->OnClicked.AddDynamic(this, &UPitchBoardWidget::OnBackgroundClicked); }

	if (Card)
	{
		Card->OnCardSelected.AddDynamic(this, &UPitchBoardWidget::HandleCardAccepted);
		Card->OnViewEmployeesRequested.AddDynamic(this, &UPitchBoardWidget::HandleViewEmployees);
	}

	// M6 미션 가이드 — 보드 열림 등록 (OpenBoard→PickInHouse 전이. 타일 지목은 하지 않는다)
	if (UMissionManagerSubsystem* MissionMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>() : nullptr)
	{
		MissionMgr->RegisterPitchBoard(this);
	}
}

void UPitchBoardWidget::NativeDestruct()
{
	if (PrevTierButton) { PrevTierButton->OnClicked().RemoveAll(this); }
	if (NextTierButton) { NextTierButton->OnClicked().RemoveAll(this); }
	if (TrendRerollButton) { TrendRerollButton->OnClicked().RemoveAll(this); }
	if (BackToRecommendButton) { BackToRecommendButton->OnClicked().RemoveAll(this); }
	if (CloseButton) { CloseButton->OnCloseClicked.RemoveDynamic(this, &UPitchBoardWidget::OnCloseClicked); }
	if (BackgroundBtn) { BackgroundBtn->OnClicked.RemoveDynamic(this, &UPitchBoardWidget::OnBackgroundClicked); }

	if (Card)
	{
		Card->OnCardSelected.RemoveDynamic(this, &UPitchBoardWidget::HandleCardAccepted);
		Card->OnViewEmployeesRequested.RemoveDynamic(this, &UPitchBoardWidget::HandleViewEmployees);
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

	// 열 때마다 현재 착석 로스터로 전부 재계산 — 저장하는 판정은 없다 (스펙 §6.2)
	Rebuild();
}

void UPitchBoardWidget::NativeOnDeactivated()
{
	Super::NativeOnDeactivated();

	// CommonUI 는 인스턴스를 재사용한다 — 리셋하지 않으면 다시 열 때 지난 티어와 지난 선택이 그대로 뜬다
	ViewTier = 0;
	SelectedIndex = INDEX_NONE;
}

UWidget* UPitchBoardWidget::GetRecommendCardCtaWidget() const
{
	return Card ? Card->GetPrimaryCtaWidget() : nullptr;
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

void UPitchBoardWidget::Rebuild()
{
	UOfficeStageProgressManager* StageMgr = GetStageMgr();
	UTableManagerSubsystem* TableMgr = GetTableMgr();
	UTrendManagerSubsystem* TrendMgr = GetTrendMgr();
	if (!StageMgr || !TableMgr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PitchBoard] 매니저 미확보 - 보드 구성 중단"));
		return;
	}

	if (UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance()))
	{
		Industry = GI->GetCurrentBuildingCompanyType();
	}

	const FProjectTierProgress& Progress = StageMgr->GetTierProgress();
	CurrentTier = Progress.CurrentTier;
	if (ViewTier <= 0)
	{
		ViewTier = CurrentTier;
	}

	const int32 ClearedCurrent = Progress.GetTierClearedCount(CurrentTier);
	const bool bNextPreview = ClearedCurrent >= PreviewThreshold;

	// 목록 전체가 같은 팀을 보므로 로스터 수집은 한 번만
	TArray<FEstimateWorkerInput> Roster;
	StageMgr->BuildContributorRoster(Roster);

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

	// 출시 기록 = 등급 글자 출처. (인덱스, 산업) 으로 찾는다 — 재개발해도 행이 하나라 모호하지 않다.
	const TArray<FShippedProjectRecord>* Shipped = nullptr;
	if (UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance()))
	{
		if (USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>())
		{
			if (USaveGame_GameData* SaveData = SaveMgr->GetCurrentSaveData())
			{
				Shipped = &SaveData->GameData.ShippedProjects;
			}
		}
	}

	int32 ViewStart = 0, ViewEnd = -1;
	FTierLayout::GetRange(ViewTier, ViewStart, ViewEnd);
	int32 NextStart = 0, NextEnd = -1;
	FTierLayout::GetRange(ViewTier + 1, NextStart, NextEnd);
	// 선착수 2개는 보고 있는 티어와 무관하게 언제나 CurrentTier+1 의 첫 2개 — ViewTier+1 로 잡으면 그 보드에서 자기 자신이 전부 잠긴다
	int32 PreviewStart = 0, PreviewEnd = -1;
	FTierLayout::GetRange(CurrentTier + 1, PreviewStart, PreviewEnd);

	const TArray<int32>& Developed = Progress.ClearedProjects;

	Rows.Reset();
	for (const FProjectData& Proj : TableMgr->GetProjectsByCompanyType(Industry))
	{
		const bool bInView = (ViewEnd >= ViewStart) && (Proj.ProjectIndex >= ViewStart && Proj.ProjectIndex <= ViewEnd);
		// 11·12칸 = 다음 티어 첫 2개
		const bool bInNext = (NextEnd >= NextStart) && (Proj.ProjectIndex >= NextStart && Proj.ProjectIndex < NextStart + PreviewTileCount);
		if (!bInView && !bInNext)
		{
			continue;
		}
		if (!Proj.IsPitchReady())
		{
			UE_LOG(LogTemp, Warning, TEXT("[PitchBoard] 미저작 행 #%d 건너뜀 - 가중치/장르 결손"), Proj.ProjectIndex);
			continue;
		}

		FBoardRow BoardRow;
		BoardRow.Project = Proj;
		BoardRow.bTrend = TrendMgr && TrendMgr->IsTrendMaterial(Industry, Proj.Material);
		BoardRow.bDeveloped = Developed.Contains(Proj.ProjectIndex);

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
		PitchSlot.bTrendMatched = BoardRow.bTrend;

		PitchSlot.ProjectName = Proj.ProjectName.IsEmpty()
			? FText::Format(NSLOCTEXT("PitchBoard", "NameCompose", "{0} {1}"), FText::FromName(Proj.Material), FText::FromName(Proj.Genre))
			: Proj.ProjectName;

		PitchSlot.RequiredScore_Step1 = Proj.GetRequiredScore(1);
		PitchSlot.RequiredScore_Step2 = Proj.GetRequiredScore(2);
		PitchSlot.RequiredScore_Step3 = Proj.GetRequiredScore(3);
		PitchSlot.RequiredScore_Step4 = Proj.GetRequiredScore(4);

		FProjectGenreRow GenreRow;
		PitchSlot.GenreColor = TableMgr->GetGenreInfo(Industry, Proj.Genre, GenreRow) ? GenreRow.Color : FLinearColor::Gray;

		// 판정·수익 단일 경로 — 출시 확인/결산서와 같은 추정기라 창구마다 다른 수치가 나오지 않는다
		const FProjectOutlookEstimate Outlook = StageMgr->EstimateProjectOutlook(Roster, Proj);
		PitchSlot.ExpectedQuality = Outlook.ExpectedQuality;
		PitchSlot.bGateRisk = Outlook.bGateRisk;
		PitchSlot.ExpectedDisciplineScores = Outlook.ExpectedDisciplineScores;
		PitchSlot.bHasOutlookEstimate = Outlook.bIsValid;

		int64 Revenue = 0;
		int32 OpSec = 0;
		StageMgr->EstimatePitchEconomy(Industry, PitchSlot.ExpectedQuality, BoardRow.bTrend, Revenue, OpSec, Proj.ProjectIndex);
		PitchSlot.EstimatedRevenue = Revenue;
		PitchSlot.EstimatedOpTimeSec = OpSec;

		const FShippedProjectRecord* Record = Shipped
			? Shipped->FindByPredicate([this, &Proj](const FShippedProjectRecord& R)
				{ return R.ProjectIndex == Proj.ProjectIndex && R.Industry == Industry; })
			: nullptr;
		BoardRow.DevelopedGrade = Record ? QualityGradeToAlphabetString(Record->QualityGrade) : FString();

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

		// 잠금은 "그 프로젝트가 실제로 착수 가능한가"로 판정한다 — 지나온 티어 보드의 11·12칸은 이미 열린 행이라 잠기지 않는다
		const int32 RowTier = FTierLayout::GetTierForProject(Proj.ProjectIndex);
		BoardRow.bFirstTwoOfNext = (PreviewEnd >= PreviewStart)
			&& (Proj.ProjectIndex >= PreviewStart)
			&& (Proj.ProjectIndex < PreviewStart + PreviewTileCount);
		const bool bPreviewRow = bNextPreview && BoardRow.bFirstTwoOfNext;
		const bool bLockedRow = (RowTier > CurrentTier) && !bPreviewRow;
		BoardRow.Verdict = PitchBoardText::ClassifyTile(BoardRow.Entry, bLockedRow, bPreviewRow);

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
	RecommendedIndex = PitchBoardText::ComputeRecommendedIndex(Entries);

	// 선택은 살아 있으면 유지(같은 보드를 다시 열어도 보던 기획안 그대로) — 없어졌으면 추천 → 가장 가까운 미달 → 첫 행
	const FBoardRow* KeptRow = FindRow(SelectedIndex);
	if (!KeptRow || KeptRow->Verdict == PitchBoardText::ETileVerdict::Locked)
	{
		SelectedIndex = RecommendedIndex;
		if (SelectedIndex == INDEX_NONE)
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

	RenderTierLine();
	RenderTiles();
	RenderCard();
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
	const TSubclassOf<UUserWidget> TileClass = TableMgr ? TableMgr->GetWidgetClass(EWidgetType::PitchTile) : nullptr;
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

		FPitchTileData TileData;
		TileData.ProjectIndex = BoardRow.Project.ProjectIndex;
		TileData.ProjectName = BoardRow.BoardSlot.ProjectName;
		TileData.Combo = FText::Format(NSLOCTEXT("PitchBoard", "Combo", "{0} · {1}"),
			FText::FromName(BoardRow.Project.Genre), FText::FromName(BoardRow.Project.Material));
		TileData.Cover = BoardRow.Project.Icon;
		TileData.GenreColor = BoardRow.BoardSlot.GenreColor;
		TileData.Verdict = BoardRow.Verdict;
		TileData.VerdictWord = MakeVerdictWord(BoardRow);
		if (BoardRow.Verdict == PitchBoardText::ETileVerdict::Locked)
		{
			// 잠금 사유가 둘이다 — 다음 티어 첫 2칸은 "몇 개 더 깨면 열린다", 나머지는 "그 티어 자체가 아직 멀다"
			TileData.LockText = BoardRow.bFirstTwoOfNext
				? FText::Format(NSLOCTEXT("PitchBoard", "LockHint", "{0}/{1} 에 해금"), PreviewThreshold, FTierLayout::ProjectsPerTier(CurrentTier))
				: FText::Format(NSLOCTEXT("PitchBoard", "LockTier", "티어 {0} 해금 후"), FTierLayout::GetTierForProject(BoardRow.Project.ProjectIndex));
		}
		TileData.DevelopedGrade = BoardRow.DevelopedGrade;
		TileData.bTrend = BoardRow.bTrend;
		TileData.bRecommended = (BoardRow.Project.ProjectIndex == RecommendedIndex);

		Tile->Configure(TileData);
		// 매 렌더 새 인스턴스라 해제 쌍이 필요 없다 — 대신 같은 타일에 두 번 바인딩하지 않는다
		Tile->OnTileClicked.AddDynamic(this, &UPitchBoardWidget::HandleTileClicked);
		TileGrid->AddChildToUniformGrid(Tile, Index / TileColumns, Index % TileColumns);

		Tiles.Add(Tile);
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

void UPitchBoardWidget::RenderCard()
{
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

	// 카드가 스스로 토글한 「자세히」를 세션 기억으로 흡수 — 안 하면 타일 전환마다 접힘으로 되돌아간다
	bCardExpanded = Card->IsExpanded();

	Card->SetDevelopedGrade(BoardRow->DevelopedGrade);
	Card->SetSlotData(BoardRow->BoardSlot, 0);
	Card->SetRecommended(BoardRow->Project.ProjectIndex == RecommendedIndex);
	Card->SetExpanded(bCardExpanded);

	bool bAnyOutlook = false;
	for (const FBoardRow& Row : Rows)
	{
		if (Row.Entry.bHasOutlook)
		{
			bAnyOutlook = true;
			break;
		}
	}

	FText Kicker;
	FLinearColor KickerColor = KickerInk2;
	bool bShowBackToRecommend = false;

	if (ViewTier == CurrentTier + 1)
	{
		// 미리보기 보드는 "지금 뭘 하나"보다 "여기선 뭘 할 수 있나"를 먼저 답해야 한다
		Kicker = NSLOCTEXT("PitchBoard", "KickerPreviewTier", "다음 티어 미리보기 · 선착수 2개");
		KickerColor = KickerGold;
	}
	else if (!bAnyOutlook)
	{
		Kicker = NSLOCTEXT("PitchBoard", "KickerNoRoster", "직원을 앉히면 판정이 표시됩니다");
	}
	else if (RecommendedIndex == INDEX_NONE)
	{
		Kicker = NSLOCTEXT("PitchBoard", "KickerNoPass", "통과하는 기획안이 없습니다 · 직원 성장 필요");
		KickerColor = KickerAlert;
	}
	else if (SelectedIndex == RecommendedIndex)
	{
		Kicker = NSLOCTEXT("PitchBoard", "KickerRecommend", "★ 추천 · 지금 팀으로 바로 출시");
		KickerColor = KickerGold;
	}
	else
	{
		Kicker = NSLOCTEXT("PitchBoard", "KickerSelected", "선택한 기획안");
		bShowBackToRecommend = true;
	}

	if (Text_Kicker)
	{
		Text_Kicker->SetText(Kicker);
		Text_Kicker->SetColorAndOpacity(FSlateColor(KickerColor));
	}
	if (BackToRecommendButton)
	{
		BackToRecommendButton->SetVisibility(bShowBackToRecommend ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UPitchBoardWidget::RenderTierLine()
{
	UOfficeStageProgressManager* StageMgr = GetStageMgr();
	if (!StageMgr)
	{
		return;
	}

	const FProjectTierProgress& Progress = StageMgr->GetTierProgress();
	const int32 ClearedView = Progress.GetTierClearedCount(ViewTier);

	if (Text_TierName)
	{
		Text_TierName->SetText(FText::Format(NSLOCTEXT("PitchBoard", "TierName", "티어 {0}"), ViewTier));
	}
	const int32 ProjectsThisTier = FTierLayout::ProjectsPerTier(ViewTier);
	if (Text_TierCount)
	{
		Text_TierCount->SetText(FText::Format(NSLOCTEXT("PitchBoard", "TierCount", "{0}/{1}"), ClearedView, ProjectsThisTier));
	}
	if (MeterBox)
	{
		// 핍은 WBP 저작물이라 레이아웃이 그보다 길어져도 못 늘린다 — 넘치는 칸을 조용히 삼키지 않게 한 번 알린다
		const int32 PipCount = MeterBox->GetChildrenCount();
		if (PipCount != ProjectsThisTier)
		{
			static bool bMeterCellsWarned = false;
			if (!bMeterCellsWarned)
			{
				bMeterCellsWarned = true;
				UE_LOG(LogTemp, Warning, TEXT("[PitchBoard] 미터 핍 %d개 != 티어 %d 프로젝트 %d개 - WBP MeterBox 갱신 필요"), PipCount, ViewTier, ProjectsThisTier);
			}
		}
		for (int32 Cell = 0; Cell < PipCount; ++Cell)
		{
			if (UImage* Pip = Cast<UImage>(MeterBox->GetChildAt(Cell)))
			{
				Pip->SetColorAndOpacity(Cell < ClearedView ? MeterOn : MeterOff);
			}
		}
	}

	if (Text_Trend)
	{
		UTrendManagerSubsystem* TrendMgr = GetTrendMgr();
		const FName TrendMaterial = TrendMgr ? TrendMgr->GetTrendMaterial(Industry) : NAME_None;
		Text_Trend->SetText(TrendMaterial.IsNone()
			? NSLOCTEXT("PitchBoard", "TrendNone", "트렌드 없음")
			: FText::Format(NSLOCTEXT("PitchBoard", "TrendChip", "트렌드 {0} · {1}회 남음"),
				FText::FromName(TrendMaterial), TrendMgr->GetLaunchesLeft(Industry)));
	}

	const int32 ClearedCurrent = Progress.GetTierClearedCount(CurrentTier);
	if (PrevTierButton)
	{
		PrevTierButton->SetIsEnabled(ViewTier > 1);
	}
	if (NextTierButton)
	{
		const bool bCanGoNext = (ViewTier < CurrentTier)
			|| (ViewTier == CurrentTier && ClearedCurrent >= PreviewThreshold && ViewTier < FTierLayout::MaxTier());
		NextTierButton->SetIsEnabled(bCanGoNext);
	}
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

FText UPitchBoardWidget::MakeVerdictWord(const FBoardRow& BoardRow) const
{
	switch (BoardRow.Verdict)
	{
	case PitchBoardText::ETileVerdict::Pass:
		return NSLOCTEXT("PitchBoard", "VerdictPass", "출시 가능");

	case PitchBoardText::ETileVerdict::Redevelop:
		return FText::FromString(FString::Printf(TEXT("재개발 · ★%s"), *BoardRow.DevelopedGrade));

	case PitchBoardText::ETileVerdict::Locked:
	case PitchBoardText::ETileVerdict::Preview:
		return NSLOCTEXT("PitchBoard", "VerdictNextTier", "다음 티어");

	default:
		break;
	}

	// 미달 — 로스터가 없어 판정 자체를 못 낸 경우는 부족 직능을 말할 수 없다(카드 밴드와 같은 문구로 맞춘다)
	if (!BoardRow.Entry.bHasOutlook || BoardRow.Entry.WorstSlot == INDEX_NONE)
	{
		return NSLOCTEXT("PitchBoard", "VerdictWait", "팀 배치 필요");
	}

	const UTableManagerSubsystem* TableMgr = GetTableMgr();
	const FText WorstName = TableMgr ? TableMgr->GetDisciplineDisplayName(Industry, BoardRow.Entry.WorstSlot) : FText::GetEmpty();
	return PitchBoardText::MakeShortageWord(WorstName);
}

void UPitchBoardWidget::HandleTileClicked(int32 ProjectIndex)
{
	const FBoardRow* BoardRow = FindRow(ProjectIndex);
	if (!BoardRow || BoardRow->Verdict == PitchBoardText::ETileVerdict::Locked)
	{
		return;
	}

	SelectedIndex = ProjectIndex;
	UpdateTileSelection();
	RenderCard();
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

void UPitchBoardWidget::HandleViewEmployees(int32 WorstSlot)
{
	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GetTableMgr();
	UUIManagerSubsystem* UIMgr = GI ? GI->GetSubsystem<UUIManagerSubsystem>() : nullptr;
	if (!TableMgr || !UIMgr || !UIMgr->GetUIBase())
	{
		return;
	}

	const TSubclassOf<UUserWidget> EmployeeWindow = TableMgr->GetWidgetClass(EWidgetType::EmployeeWindow);
	if (!EmployeeWindow)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PitchBoard] EmployeeWindow 클래스 미등록 - DT_WidgetClass 확인"));
		return;
	}

	// 부족 직능 포커스는 직원창 API 가 없어 이번엔 열기만 한다(보드는 아래에 남는다)
	UE_LOG(LogTemp, Log, TEXT("[PitchBoard] 직원 보기 - 부족 직능 슬롯 %d"), WorstSlot);

	UIMgr->GetUIBase()->PushPromptClass(EmployeeWindow.Get());

	// 입력모드 쌍 — 닫힐 때 EmployeeWindow::NativeOnDeactivated 가 GoToNormalMode 복원
	if (UCGGameInstance* CGGI = Cast<UCGGameInstance>(GI))
	{
		if (AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(CGGI->GetCurrentPlayerController()))
		{
			PC->GoToUIMode();
		}
	}
}

void UPitchBoardWidget::HandleBackToRecommend()
{
	if (RecommendedIndex == INDEX_NONE)
	{
		return;
	}

	SelectedIndex = RecommendedIndex;
	UpdateTileSelection();
	RenderCard();
}

void UPitchBoardWidget::HandlePrevTier()
{
	OpenTier(ViewTier - 1);
}

void UPitchBoardWidget::HandleNextTier()
{
	OpenTier(ViewTier + 1);
}

void UPitchBoardWidget::HandleTrendReroll()
{
	UTrendManagerSubsystem* TrendMgr = GetTrendMgr();
	UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance());
	UItemInventoryManager* ItemMgr = GI ? GI->GetSubsystem<UItemInventoryManager>() : nullptr;
	if (!TrendMgr || !ItemMgr)
	{
		// 인벤토리를 못 잡으면 차감할 곳이 없다 — 공짜 갱신을 내주느니 아무것도 하지 않는다
		return;
	}

	// 마지막 Rebuild 의 값을 믿지 않는다 — 갱신 대상은 지금 들어와 있는 건물의 산업이다
	const ECompanyType RerollIndustry = GI->GetCurrentBuildingCompanyType();
	UUIManagerSubsystem* UIMgr = GI->GetSubsystem<UUIManagerSubsystem>();

	// 보유만 먼저 보고 차감은 재추첨 성공 뒤로 미룬다 — 소재 풀이 1개면 트렌드가 그대로라 소모도 없어야 한다
	if (!ItemMgr->HasItem(EItemType::PitchRefreshScroll, 1))
	{
		if (UIMgr)
		{
			UIMgr->ShowNotification(NSLOCTEXT("PitchBoard", "NoTrendScroll", "트렌드 갱신 스크롤이 부족합니다"), 3.0f, ENotificationType::Failed);
		}
		return;
	}

	if (!TrendMgr->RerollTrend(RerollIndustry))
	{
		if (UIMgr)
		{
			UIMgr->ShowNotification(NSLOCTEXT("PitchBoard", "TrendRerollEmpty", "갱신할 다른 소재가 없습니다"), 3.0f, ENotificationType::Failed);
		}
		return;
	}

	ItemMgr->SpendItem(EItemType::PitchRefreshScroll, 1);
	Rebuild();
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

UTrendManagerSubsystem* UPitchBoardWidget::GetTrendMgr() const
{
	return GetGameInstance() ? GetGameInstance()->GetSubsystem<UTrendManagerSubsystem>() : nullptr;
}
