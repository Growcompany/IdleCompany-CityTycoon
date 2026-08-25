#include "UI/Panel/CodexPanelWidget.h"

#include "Core/CGGameInstance.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/OfficeStageProgressManager.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/PlayFabManagerSubsystem.h"
#include "Data/GameSaveData.h"
#include "Data/ShippedRecord.h"
#include "Data/ProjectBoardData.h"
#include "Data/BuildingSaveData.h"
#include "Table/CompanyInfoTable.h"
#include "Table/ProjectGenreTable.h"
#include "Table/ProjectDataTable.h"
#include "Enum/QualityGrade.h"
#include "Global/GlobalUtilFunctions.h"
#include "Engine/Texture2D.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "UI/Element/Portfolio/RoadmapNodeWidget.h"
#include "UI/Element/Portfolio/PortfolioCellWidget.h"

#include "CommonTextBlock.h"
#include "Groups/CommonButtonGroupBase.h"
#include "CommonButtonBase.h"
#include "Components/WidgetSwitcher.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/Image.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/UserWidget.h"
#include "Engine/Font.h"

namespace
{
	FLinearColor Lighten(const FLinearColor& In, float Amount)
	{
		return FMath::Lerp(In, FLinearColor::White, Amount);
	}

	// 랭크 칭호 = DT_TierRankTitle. 매핑 없으면(타 산업 미입력) 중립 폴백 "N티어 회사"(산업별 하드코딩 아님).
	FText ResolveRankTitle(UTableManagerSubsystem* TableMgr, ECompanyType Industry, int32 Tier)
	{
		const FText DtTitle = TableMgr ? TableMgr->GetTierRankTitle(Industry, Tier) : FText::GetEmpty();
		if (!DtTitle.IsEmpty()) { return DtTitle; }
		return FText::FromString(FString::Printf(TEXT("%d티어 회사"), Tier));
	}
}

void UCodexPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 코드 생성 텍스트용 폰트 (TSoftObjectPtr+LoadSynchronous — 모바일 쿠킹 안전 규칙)
	BoldFont = TSoftObjectPtr<UFont>(FSoftObjectPath(
		TEXT("/Game/CompanyGrowth/Font/NEXONLv1GothicBold_Font.NEXONLv1GothicBold_Font"))).LoadSynchronous();
	RegularFont = TSoftObjectPtr<UFont>(FSoftObjectPath(
		TEXT("/Game/CompanyGrowth/Font/NEXONLv1GothicRegular_Font.NEXONLv1GothicRegular_Font"))).LoadSynchronous();

	if (UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance()))
	{
		Industry = GI->GetCurrentBuildingCompanyType();
	}
	// 현재 티어 — OfficeMap 은 WorldSubsystem, MainMap 등은 영속 세이브(OfficeDataMap)에서 폴백
	if (UOfficeStageProgressManager* StageMgr = GetWorld() ? GetWorld()->GetSubsystem<UOfficeStageProgressManager>() : nullptr)
	{
		CurrentTier = StageMgr->GetTierProgress().CurrentTier;
	}
	else if (UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance()))
	{
		const int32 BuildingIndex = GI->GetCurrentManagedBuildingIndex();
		if (BuildingIndex >= 0)
		{
			if (USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>())
			{
				if (USaveGame_GameData* SaveData = SaveMgr->GetCurrentSaveData())
				{
					if (const FOfficeSaveData* Office = SaveData->GameData.OfficeDataMap.Find(BuildingIndex))
					{
						CurrentTier = Office->TierProgress.CurrentTier;
					}
				}
			}
		}
	}
	SelectedTier = CurrentTier;

	ApplyCompanyIdentity();

	// 탭 배타선택 — 전부 NativeConstruct (playbook)
	TabGroup = NewObject<UCommonButtonGroupBase>(this);
	TabGroup->SetSelectionRequired(true);
	UButtonWidget* Tabs[2] = { Tab_Discovery, Tab_Shipped };
	for (UButtonWidget* Tab : Tabs)
	{
		if (Tab)
		{
			TabGroup->AddWidget(Tab);
			Tab->SetIsSelectable(true);
		}
	}
	TabGroup->OnSelectedButtonBaseChanged.AddDynamic(this, &UCodexPanelWidget::OnTabChanged);
	TabGroup->SelectButtonAtIndex(0);
	if (CodexSwitcher) { CodexSwitcher->SetActiveWidgetIndex(0); }

	// 출시작 정렬 토글 (평점순/매출순) — 전부 NativeConstruct (playbook)
	if (SortByReviewBtn || SortByRevenueBtn)
	{
		SortGroup = NewObject<UCommonButtonGroupBase>(this);
		SortGroup->SetSelectionRequired(true);
		UButtonWidget* SortBtns[2] = { SortByReviewBtn, SortByRevenueBtn };
		for (UButtonWidget* SortBtn : SortBtns)
		{
			if (SortBtn)
			{
				SortGroup->AddWidget(SortBtn);
				SortBtn->SetIsSelectable(true);
			}
		}
		SortGroup->OnSelectedButtonBaseChanged.AddDynamic(this, &UCodexPanelWidget::OnShippedSortChanged);
		SortGroup->SelectButtonAtIndex(0);
	}

	if (CloseButton)
	{
		CloseButton->OnCloseClicked.AddDynamic(this, &UCodexPanelWidget::OnCloseDelegate);
	}
	if (BackgroundBtn)
	{
		BackgroundBtn->OnClicked.AddDynamic(this, &UCodexPanelWidget::OnBackgroundClicked);
	}

	RefreshRankSummary();
	PopulateRoadmap();
	RefreshTierDetail();
	PopulateShipped();
}

void UCodexPanelWidget::NativeDestruct()
{
	if (TabGroup)
	{
		TabGroup->OnSelectedButtonBaseChanged.RemoveAll(this);
		TabGroup->RemoveAll();
	}
	if (SortGroup)
	{
		SortGroup->OnSelectedButtonBaseChanged.RemoveAll(this);
		SortGroup->RemoveAll();
	}
	if (CloseButton) { CloseButton->OnCloseClicked.RemoveDynamic(this, &UCodexPanelWidget::OnCloseDelegate); }
	if (BackgroundBtn) { BackgroundBtn->OnClicked.RemoveDynamic(this, &UCodexPanelWidget::OnBackgroundClicked); }
	Super::NativeDestruct();
}

UCommonTextBlock* UCodexPanelWidget::MakeText(const FText& InText, int32 Size, const FLinearColor& Color, bool bBold) const
{
	UCommonTextBlock* Text = WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass());
	Text->SetText(InText);
	if (UFont* FontObj = bBold ? BoldFont : RegularFont)
	{
		Text->SetFont(FSlateFontInfo(FontObj, Size));
	}
	Text->SetColorAndOpacity(FSlateColor(Color));
	return Text;
}

void UCodexPanelWidget::ApplyCompanyIdentity()
{
	UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance());
	if (!GI) { return; }

	if (UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>())
	{
		bool bFound = false;
		const FCompanyInfoTable Info = TableMgr->GetCompanyInfo(Industry, bFound);
		if (bFound)
		{
			IndustryColor = Info.AccentColor;
			if (CompanyIndustryText) { CompanyIndustryText->SetText(Info.DisplayName); }
		}
	}

	// 산업색 주입 (브러시 base tint=흰색, ColorAndOpacity 로 곱셈 틴트)
	if (AccentBar) { AccentBar->SetColorAndOpacity(IndustryColor); }
	if (CompanyPlate) { CompanyPlate->SetColorAndOpacity(IndustryColor); }

	if (CompanyNameText)
	{
		FString Name;
		if (UPlayFabManagerSubsystem* PlayFabMgr = GI->GetSubsystem<UPlayFabManagerSubsystem>())
		{
			if (PlayFabMgr->IsLoggedIn()) { Name = PlayFabMgr->GetUserInfo().DisplayName; }
		}
		CompanyNameText->SetText(FText::FromString(Name.IsEmpty() ? TEXT("내 회사") : Name));
	}
}

void UCodexPanelWidget::RefreshRankSummary()
{
	if (Text_RankTier)
	{
		Text_RankTier->SetText(FText::FromString(FString::Printf(TEXT("TIER %d"), CurrentTier)));
		Text_RankTier->SetColorAndOpacity(FSlateColor(Lighten(IndustryColor, 0.35f)));
	}
	if (Text_RankTitle)
	{
		UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
		Text_RankTitle->SetText(ResolveRankTitle(TableMgr, Industry, CurrentTier));
	}
}

void UCodexPanelWidget::PopulateRoadmap()
{
	// 진척 채움 라인 — 현재 티어까지 비율로 폭(RenderScale.X) + 산업색
	if (RoadmapFillLine)
	{
		const float Frac = (TierConstants::MAX_TIER > 1)
			? FMath::Clamp(float(CurrentTier - 1) / float(TierConstants::MAX_TIER - 1), 0.f, 1.f) : 0.f;
		FWidgetTransform FillT;
		FillT.Scale = FVector2D(Frac, 1.f);
		RoadmapFillLine->SetRenderTransform(FillT);
		RoadmapFillLine->SetColorAndOpacity(Lighten(IndustryColor, 0.12f));
	}

	if (!RoadmapBox) { return; }
	RoadmapBox->ClearChildren();

	// 노드 = 재사용 컴포넌트(UIE_RoadmapNode). 생김새는 WBP, 상태/색만 Configure 로 주입.
	// TODO(데이터 배선): 노드 클릭→SelectedTier 전환 + 티어 완성 여부(초록 체크).
	UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	TSubclassOf<UUserWidget> NodeClass = TableMgr ? TableMgr->GetWidgetClass(EWidgetType::RoadmapNode) : nullptr;
	if (!NodeClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CodexPanel] RoadmapNode 위젯 미등록(DT_WidgetClass)"));
		return;
	}

	for (int32 TierIndex = 1; TierIndex <= TierConstants::MAX_TIER; ++TierIndex)
	{
		URoadmapNodeWidget* Node = CreateWidget<URoadmapNodeWidget>(this, NodeClass);
		if (!Node) { continue; }

		const ERoadmapNodeState State = (TierIndex < CurrentTier) ? ERoadmapNodeState::Done
			: (TierIndex == CurrentTier) ? ERoadmapNodeState::Current : ERoadmapNodeState::Upcoming;
		Node->Configure(TierIndex, State, IndustryColor);

		if (UHorizontalBoxSlot* NodeSlot = RoadmapBox->AddChildToHorizontalBox(Node))
		{
			NodeSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			NodeSlot->SetHorizontalAlignment(HAlign_Center);
			NodeSlot->SetVerticalAlignment(VAlign_Top);
		}
	}
}

void UCodexPanelWidget::RefreshTierDetail()
{
	const bool bDone = SelectedTier < CurrentTier;
	const bool bCur = SelectedTier == CurrentTier;

	if (Text_TierRank)
	{
		UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
		Text_TierRank->SetText(ResolveRankTitle(TableMgr, Industry, SelectedTier));
	}
	if (Text_TierReward)
	{
		// 실제 코드가 지급하는 것만 표기 — 티어 해금(CLEAR_TO_UNLOCK/PROJECTS_PER_TIER) + 10/10 도감 다이아.
		// 다이아 수량은 OfficeStageProgressManager 의 private 상수라 여기서 숫자를 복제하지 않는다.
		const FText CurrentReward = FText::Format(
			NSLOCTEXT("Codex", "TierRewardCurrent", "{0}/{1} 개발 시 다음 티어가 열립니다 · 전부 개발하면 다이아를 받습니다"),
			FText::AsNumber(TierConstants::CLEAR_TO_UNLOCK), FText::AsNumber(TierConstants::PROJECTS_PER_TIER));

		Text_TierReward->SetText(bCur ? CurrentReward
			: (bDone ? FText::FromString(TEXT("지나온 티어입니다 ― 다시 개발할 수 있습니다"))
				: FText::FromString(TEXT("현재 티어를 완료하면 열립니다"))));
	}

	// Text_TierProgress(발견 카운트)는 실제 조합을 순회하는 PopulateTierGrid 가 채움
	PopulateTierGrid();
}

void UCodexPanelWidget::ResolveDevelopedProjects(TArray<int32>& OutDeveloped) const
{
	OutDeveloped.Reset();

	// 발견셋 = 자체개발 클리어 기록. 오피스 WorldSubsystem 우선, MainMap 등은 세이브 OfficeDataMap 폴백.
	if (UOfficeStageProgressManager* StageMgr = GetWorld() ? GetWorld()->GetSubsystem<UOfficeStageProgressManager>() : nullptr)
	{
		OutDeveloped = StageMgr->GetTierProgress().ClearedProjects;
		return;
	}

	if (UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance()))
	{
		const int32 BuildingIndex = GI->GetCurrentManagedBuildingIndex();
		if (BuildingIndex >= 0)
		{
			if (USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>())
			{
				if (USaveGame_GameData* SaveData = SaveMgr->GetCurrentSaveData())
				{
					if (const FOfficeSaveData* Office = SaveData->GameData.OfficeDataMap.Find(BuildingIndex))
					{
						OutDeveloped = Office->TierProgress.ClearedProjects;
					}
				}
			}
		}
	}
}

void UCodexPanelWidget::PopulateTierGrid()
{
	if (!DexGroupsBox) { return; }
	DexGroupsBox->ClearChildren();

	UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	TSubclassOf<UUserWidget> CellClass = TableMgr ? TableMgr->GetWidgetClass(EWidgetType::PortfolioCell) : nullptr;
	if (!TableMgr || !CellClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CodexPanel] PortfolioCell/TableMgr 미준비"));
		return;
	}

	// per-project 도감 = 선택 티어의 프로젝트를 장르 그룹으로 열람. 착수는 기획 보드 담당(여긴 표시만).
	TArray<int32> Developed;
	ResolveDevelopedProjects(Developed);
	TArray<FProjectTierEntry> Entries;
	UOfficeStageProgressManager::BuildTierProjectEntries(Industry, SelectedTier, CurrentTier, Developed, Entries);

	// 등급 출처 = 그 작품의 출시 기록. 미개발 셀은 기록이 없어 등급 없이(= '?' 만) 뜬다.
	const TArray<FShippedProjectRecord>* Shipped = nullptr;
	if (UCGGameInstance* ShipGI = UCGGameInstance::GetInstance())
	{
		if (USaveLoadManager* SaveMgr = ShipGI->GetSubsystem<USaveLoadManager>())
		{
			if (USaveGame_GameData* SaveData = SaveMgr->GetCurrentSaveData())
			{
				Shipped = &SaveData->GameData.ShippedProjects;
			}
		}
	}

	TArray<FName> GenreOrder;   // 등장 순서 보존
	TMap<FName, TArray<FProjectTierEntry>> ByGenre;
	for (const FProjectTierEntry& E : Entries)
	{
		if (!ByGenre.Contains(E.Genre)) { GenreOrder.Add(E.Genre); }
		ByGenre.FindOrAdd(E.Genre).Add(E);
	}

	const FLinearColor HeaderColor(0.840f, 0.857f, 0.871f, 1.0f);
	int32 TotalCount = 0;
	int32 DevelopedCount = 0;

	for (const FName& Genre : GenreOrder)
	{
		UVerticalBox* Group = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());

		if (UVerticalBoxSlot* HeaderSlot = Group->AddChildToVerticalBox(
			MakeText(FText::FromName(Genre), 30, HeaderColor, true)))
		{
			HeaderSlot->SetPadding(FMargin(4.f, 0.f, 4.f, 12.f));
		}

		UWrapBox* CellRow = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass());
		CellRow->SetInnerSlotPadding(FVector2D(14.f, 14.f));

		for (const FProjectTierEntry& E : ByGenre[Genre])
		{
			const FShippedProjectRecord* Rec = Shipped
				? Shipped->FindByPredicate([&E, this](const FShippedProjectRecord& R)
					{ return R.ProjectIndex == E.ProjectIndex && R.Industry == Industry; })
				: nullptr;
			const FString GradeStr = Rec ? QualityGradeToAlphabetString(Rec->QualityGrade) : FString();
			++TotalCount;

			UPortfolioCellWidget* Cell = CreateWidget<UPortfolioCellWidget>(this, CellClass);
			if (!Cell) { continue; }

			// 열람 전용 — 어느 상태에서도 OnCellClicked 를 바인딩하지 않는다(착수 = 기획 보드 단일 창구).
			switch (E.State)
			{
			case EProjectEntryState::Discovered:
			{
				++DevelopedCount;
				UTexture2D* IconTex = E.Cover.IsNull() ? nullptr : E.Cover.LoadSynchronous();
				const FText Name = E.ProjectName.IsEmpty() ? FText::FromName(E.Material) : E.ProjectName;
				Cell->ConfigureDiscovered(Name, GradeStr, FText::GetEmpty(), IndustryColor, IconTex);
				break;
			}
			case EProjectEntryState::Undiscovered:
				Cell->ConfigureUndiscovered(TEXT("?"), FText::FromName(E.Material));
				break;
			case EProjectEntryState::Locked:
			default:
				Cell->ConfigureLocked(TEXT("?"));
				break;
			}
			CellRow->AddChildToWrapBox(Cell);
		}

		Group->AddChildToVerticalBox(CellRow);

		if (UVerticalBoxSlot* GroupSlot = DexGroupsBox->AddChildToVerticalBox(Group))
		{
			GroupSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 30.f));
		}
	}

	if (Text_TierProgress)
	{
		Text_TierProgress->SetText(FText::FromString(FString::Printf(TEXT("TIER %d · %d / %d 개발"),
			SelectedTier, DevelopedCount, TotalCount)));
		const bool bAllFound = (TotalCount > 0 && DevelopedCount >= TotalCount);
		const FLinearColor ProgColor = bAllFound ? FLinearColor(0.498f, 0.839f, 0.627f, 1.f)
			: Lighten(IndustryColor, 0.30f);
		Text_TierProgress->SetColorAndOpacity(FSlateColor(ProgColor));
	}
}

void UCodexPanelWidget::PopulateShipped()
{
	if (!ShippedList) { return; }
	ShippedList->ClearChildren();

	// 크림 카드 색 토큰 (linear)
	const FLinearColor CreamFill(0.906f, 0.839f, 0.694f, 1.0f);
	const FLinearColor CreamHairline(0.36f, 0.28f, 0.14f, 0.40f);
	const FLinearColor Ink(0.020f, 0.032f, 0.052f, 1.0f);
	const FLinearColor InkSub(0.230f, 0.170f, 0.095f, 1.0f);

	USaveGame_GameData* SaveData = nullptr;
	if (UCGGameInstance* GI = UCGGameInstance::GetInstance())
	{
		if (USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>())
		{
			SaveData = SaveMgr->GetCurrentSaveData();
		}
	}

	// 이 산업 출시작 수집 → 토글 정렬(평점순/매출순)로 순위 부여.
	TArray<FShippedProjectRecord> Rows;
	if (SaveData)
	{
		for (const FShippedProjectRecord& Rec : SaveData->GameData.ShippedProjects)
		{
			if (Rec.Industry == Industry) { Rows.Add(Rec); }
		}
	}
	if (ShippedSort == EShippedSort::ByRevenue)
	{
		Rows.Sort([](const FShippedProjectRecord& A, const FShippedProjectRecord& B) { return A.CumulativeRevenue > B.CumulativeRevenue; });
	}
	else
	{
		Rows.Sort([](const FShippedProjectRecord& A, const FShippedProjectRecord& B) { return A.ReviewScore > B.ReviewScore; });
	}

	if (Rows.Num() == 0)
	{
		UBorder* EmptyCard = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		FSlateBrush EmptyBrush;
		EmptyBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
		EmptyBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		EmptyBrush.OutlineSettings.CornerRadii = FVector4(16.f, 16.f, 16.f, 16.f);
		EmptyBrush.OutlineSettings.Color = FSlateColor(CreamHairline);
		EmptyBrush.OutlineSettings.Width = 1.f;
		EmptyBrush.TintColor = FSlateColor(CreamFill);
		EmptyCard->SetBrush(EmptyBrush);
		EmptyCard->SetPadding(FMargin(28.f));
		EmptyCard->SetHorizontalAlignment(HAlign_Center);
		EmptyCard->SetContent(MakeText(FText::FromString(TEXT("아직 출시한 작품이 없습니다")), 24, InkSub, false));
		ShippedList->AddChild(EmptyCard);
		return;
	}

	// 라운드박스 pill (라벨 + 배경색 + 글자색)
	auto MakePill = [this](const FText& Label, const FLinearColor& Bg, const FLinearColor& TextCol) -> UWidget*
	{
		UBorder* Pill = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		FSlateBrush PillBrush;
		PillBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
		PillBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		PillBrush.OutlineSettings.CornerRadii = FVector4(9.f, 9.f, 9.f, 9.f);
		PillBrush.TintColor = FSlateColor(Bg);
		Pill->SetBrush(PillBrush);
		Pill->SetPadding(FMargin(12.f, 3.f, 12.f, 4.f));
		Pill->SetContent(MakeText(Label, 18, TextCol, true));
		return Pill;
	};

	for (int32 RankIndex = 0; RankIndex < Rows.Num(); ++RankIndex)
	{
		const FShippedProjectRecord& Rec = Rows[RankIndex];
		const int32 Rank = RankIndex + 1;
		const bool bTop = (Rank == 1);

		// 크림 카드
		UBorder* Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		FSlateBrush CardBrush;
		CardBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
		CardBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		CardBrush.OutlineSettings.CornerRadii = FVector4(16.f, 16.f, 16.f, 16.f);
		CardBrush.OutlineSettings.Color = FSlateColor(CreamHairline);
		CardBrush.OutlineSettings.Width = 1.f;
		CardBrush.TintColor = FSlateColor(CreamFill);
		Card->SetBrush(CardBrush);
		Card->SetPadding(FMargin(22.f, 16.f, 24.f, 16.f));

		UHorizontalBox* RowBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

		// 순위 배지 (1위 = 산업색, 그 외 다크 슬레이트)
		USizeBox* RankBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		RankBox->SetWidthOverride(58.f);
		RankBox->SetHeightOverride(58.f);
		UBorder* RankBadge = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		FSlateBrush RankBrush;
		RankBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
		RankBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		RankBrush.OutlineSettings.CornerRadii = FVector4(13.f, 13.f, 13.f, 13.f);
		RankBrush.TintColor = FSlateColor(bTop ? IndustryColor : FLinearColor(0.180f, 0.210f, 0.270f, 1.0f));
		RankBadge->SetBrush(RankBrush);
		RankBadge->SetHorizontalAlignment(HAlign_Center);
		RankBadge->SetVerticalAlignment(VAlign_Center);
		RankBadge->SetContent(MakeText(FText::AsNumber(Rank), 28, FLinearColor::White, true));
		RankBox->SetContent(RankBadge);
		if (UHorizontalBoxSlot* RankSlot = RowBox->AddChildToHorizontalBox(RankBox))
		{
			RankSlot->SetPadding(FMargin(0.f, 0.f, 20.f, 0.f));
			RankSlot->SetVerticalAlignment(VAlign_Center);
		}

		// 메인: 이름(크게, 잉크) + 메타 pill(장르·소재 / 등급)
		UVerticalBox* Main = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Main->AddChildToVerticalBox(MakeText(FText::FromString(Rec.ProjectName), 28, Ink, true));

		UHorizontalBox* Meta = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UHorizontalBoxSlot* ComboSlot = Meta->AddChildToHorizontalBox(MakePill(
			FText::FromString(FString::Printf(TEXT("%s · %s"), *Rec.Genre.ToString(), *Rec.Material.ToString())),
			FLinearColor(IndustryColor.R, IndustryColor.G, IndustryColor.B, 0.14f), InkSub)))
		{
			ComboSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
		}
		const FString ShippedGrade = QualityGradeToAlphabetString(Rec.QualityGrade);
		Meta->AddChildToHorizontalBox(MakePill(FText::FromString(ShippedGrade), GradeLetterToColor(ShippedGrade), FLinearColor::White));
		if (UVerticalBoxSlot* MetaSlot = Main->AddChildToVerticalBox(Meta))
		{
			MetaSlot->SetPadding(FMargin(0.f, 9.f, 0.f, 0.f));
		}
		if (UHorizontalBoxSlot* MainSlot = RowBox->AddChildToHorizontalBox(Main))
		{
			MainSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			MainSlot->SetVerticalAlignment(VAlign_Center);
		}

		// 누적매출 kv (우측 정렬 라벨/값)
		UVerticalBox* RevKv = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		if (UVerticalBoxSlot* RevLblSlot = RevKv->AddChildToVerticalBox(MakeText(FText::FromString(TEXT("누적매출")), 16, InkSub, false)))
		{
			RevLblSlot->SetHorizontalAlignment(HAlign_Right);
		}
		if (UVerticalBoxSlot* RevValSlot = RevKv->AddChildToVerticalBox(MakeText(
			FText::Format(NSLOCTEXT("Codex", "ShipRevenue", "{0}"), UGlobalUtilFunctions::AbbreviateNumber(Rec.CumulativeRevenue)),
			26, Ink, true)))
		{
			RevValSlot->SetHorizontalAlignment(HAlign_Right);
			RevValSlot->SetPadding(FMargin(0.f, 2.f, 0.f, 0.f));
		}
		if (UHorizontalBoxSlot* RevSlot = RowBox->AddChildToHorizontalBox(RevKv))
		{
			RevSlot->SetPadding(FMargin(0.f, 0.f, 26.f, 0.f));
			RevSlot->SetVerticalAlignment(VAlign_Center);
		}

		// 평점 배지 (/40)
		USizeBox* ScoreBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		ScoreBox->SetWidthOverride(120.f);
		ScoreBox->SetHeightOverride(72.f);
		UBorder* ScoreBadge = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		ScoreBadge->SetPadding(FMargin(10.f, 8.f, 10.f, 8.f));
		FSlateBrush ScoreBrush;
		ScoreBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
		ScoreBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		ScoreBrush.OutlineSettings.CornerRadii = FVector4(14.f, 14.f, 14.f, 14.f);
		ScoreBrush.OutlineSettings.Color = FSlateColor(FLinearColor(IndustryColor.R, IndustryColor.G, IndustryColor.B, 0.45f));
		ScoreBrush.OutlineSettings.Width = 1.f;
		ScoreBrush.TintColor = FSlateColor(FLinearColor(0.160f, 0.110f, 0.240f, 1.0f));
		ScoreBadge->SetBrush(ScoreBrush);
		ScoreBadge->SetHorizontalAlignment(HAlign_Center);
		ScoreBadge->SetVerticalAlignment(VAlign_Center);
		UVerticalBox* ScoreVB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		if (UVerticalBoxSlot* ScoreNumSlot = ScoreVB->AddChildToVerticalBox(
			MakeText(FText::FromString(FString::Printf(TEXT("%d/40"), Rec.ReviewScore)), 26,
				FLinearColor(0.902f, 0.812f, 0.984f, 1.0f), true)))
		{
			ScoreNumSlot->SetHorizontalAlignment(HAlign_Center);
		}
		if (UVerticalBoxSlot* ScoreLblSlot = ScoreVB->AddChildToVerticalBox(
			MakeText(FText::FromString(TEXT("평점")), 15, FLinearColor(0.660f, 0.510f, 0.800f, 1.0f), false)))
		{
			ScoreLblSlot->SetHorizontalAlignment(HAlign_Center);
			ScoreLblSlot->SetPadding(FMargin(0.f, 2.f, 0.f, 0.f));
		}
		ScoreBadge->SetContent(ScoreVB);
		ScoreBox->SetContent(ScoreBadge);
		if (UHorizontalBoxSlot* ScoreSlot = RowBox->AddChildToHorizontalBox(ScoreBox))
		{
			ScoreSlot->SetVerticalAlignment(VAlign_Center);
		}

		Card->SetContent(RowBox);
		if (UScrollBoxSlot* CardSlot = Cast<UScrollBoxSlot>(ShippedList->AddChild(Card)))
		{
			CardSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 14.f));
		}
	}
}

void UCodexPanelWidget::OnTabChanged(UCommonButtonBase* /*SelectedButton*/, int32 ButtonIndex)
{
	if (CodexSwitcher)
	{
		CodexSwitcher->SetActiveWidgetIndex(ButtonIndex);
	}
}

void UCodexPanelWidget::OnShippedSortChanged(UCommonButtonBase* /*SelectedButton*/, int32 ButtonIndex)
{
	// 0=평점순 / 1=매출순
	ShippedSort = (ButtonIndex == 1) ? EShippedSort::ByRevenue : EShippedSort::ByReview;
	PopulateShipped();
}

void UCodexPanelWidget::OnCloseDelegate()
{
	DeactivateWidget();
}

void UCodexPanelWidget::OnBackgroundClicked()
{
	DeactivateWidget();
}
