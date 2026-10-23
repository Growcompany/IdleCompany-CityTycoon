#include "UI/Panel/CodexPanelWidget.h"
#include "UI/Element/Common/ModalFrameWidget.h"
#include "Player/MainMapPlayerController.h"

#include "Core/CGGameInstance.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/OfficeStageProgressManager.h"
#include "Manager/SaveLoadManager.h"
#include "Data/GameSaveData.h"
#include "Data/CodexIndustrySummary.h"
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
#include "UI/Element/Buttons/IconWithButtonWidget.h"
#include "UI/Element/Portfolio/PortfolioCellWidget.h"

#include "CommonTextBlock.h"
#include "Groups/CommonButtonGroupBase.h"
#include "CommonButtonBase.h"
#include "Components/PanelWidget.h"
#include "Components/PanelSlot.h"
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
#include "Framework/Text/TextLayout.h"   // ETextOverflowPolicy ― 이 리포 최초 사용이라 전이 include 에 기대지 않는다

namespace
{
	FLinearColor Lighten(const FLinearColor& In, float Amount)
	{
		return FMath::Lerp(In, FLinearColor::White, Amount);
	}

	// R2 로 티어 칭호("이제 막 진출한 게임회사")를 이 패널에서 걷어내면서 ResolveRankTitle 도 함께 제거했다.
	// 한 회사 시점 문구는 전 산업 자료집의 목소리가 아니다. DT_TierRankTitle 과 GetTierRankTitle 은
	// 다른 화면이 쓰므로 그대로 둔다.
}

void UCodexPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 코드 생성 텍스트용 폰트 (TSoftObjectPtr+LoadSynchronous — 모바일 쿠킹 안전 규칙)
	BoldFont = TSoftObjectPtr<UFont>(FSoftObjectPath(
		TEXT("/Game/CompanyGrowth/Font/NEXONLv1GothicBold_Font.NEXONLv1GothicBold_Font"))).LoadSynchronous();
	RegularFont = TSoftObjectPtr<UFont>(FSoftObjectPath(
		TEXT("/Game/CompanyGrowth/Font/NEXONLv1GothicRegular_Font.NEXONLv1GothicRegular_Font"))).LoadSynchronous();

	// 표시 산업의 권위는 레일이다. 오피스에서 열면 그 건물 산업을 초기 선택으로만 쓴다.
	ECompanyType InitialIndustry = ECompanyType::None;
	if (UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance()))
	{
		InitialIndustry = GI->GetCurrentBuildingCompanyType();
	}

	PopulateIndustryRail();

	int32 InitialIndex = RailIndustries.IndexOfByKey(InitialIndustry);
	if (InitialIndex == INDEX_NONE)
	{
		// 메뉴에서 연 경우 ― 첫 보유 산업. 하나도 없으면 첫 줄.
		InitialIndex = RailIndustries.IndexOfByPredicate([this](ECompanyType Candidate)
		{
			return OwnedIndustries.Contains(Candidate);
		});
		if (InitialIndex == INDEX_NONE) { InitialIndex = 0; }
	}
	if (RailGroup && RailIndustries.IsValidIndex(InitialIndex))
	{
		RailGroup->SelectButtonAtIndex(InitialIndex);
	}
	// 폴백 ― 초기 탭이 선택 불가거나 레일이 비어 델리게이트가 안 뜬 경우에만 직접 적용한다
	if (!bIndustryApplied)
	{
		SelectIndustry(RailIndustries.IsValidIndex(InitialIndex) ? RailIndustries[InitialIndex] : ECompanyType::Game);
	}

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

	if (UCloseButtonWidget* PanelCloseButton = ModalFrame ? ModalFrame->GetCloseButton() : nullptr)
	{
		PanelCloseButton->OnCloseClicked.AddDynamic(this, &UCodexPanelWidget::OnCloseDelegate);
	}
	if (BackgroundBtn)
	{
		BackgroundBtn->OnClicked.AddDynamic(this, &UCodexPanelWidget::OnBackgroundClicked);
	}
}

void UCodexPanelWidget::NativeDestruct()
{
	if (TabGroup)
	{
		TabGroup->OnSelectedButtonBaseChanged.RemoveAll(this);
		TabGroup->RemoveAll();
	}
	if (RailGroup)
	{
		RailGroup->OnSelectedButtonBaseChanged.RemoveAll(this);
		RailGroup->RemoveAll();
	}
	if (SortGroup)
	{
		SortGroup->OnSelectedButtonBaseChanged.RemoveAll(this);
		SortGroup->RemoveAll();
	}
	if (UCloseButtonWidget* PanelCloseButton = ModalFrame ? ModalFrame->GetCloseButton() : nullptr) { PanelCloseButton->OnCloseClicked.RemoveDynamic(this, &UCodexPanelWidget::OnCloseDelegate); }
	if (BackgroundBtn) { BackgroundBtn->OnClicked.RemoveDynamic(this, &UCodexPanelWidget::OnBackgroundClicked); }
	Super::NativeDestruct();
}

// 모드 복원은 표시 상태의 짝인 Deactivate 에서 ― Destruct 는 풀링 때문에 훨씬 늦게 올 수 있다
void UCodexPanelWidget::NativeOnDeactivated()
{
	// 메뉴 체인은 다음 창이 닫힐 때까지 UI 모드를 유지한다 ― 단독 모달이므로 여기서 해제(짝: MenuPanel 은 이어 열 때 생략)
	if (AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr))
	{
		if (PC->GetCurrentInputMode() == EInputMode::UI)
		{
			PC->GoToNormalMode();
		}
	}
	Super::NativeOnDeactivated();
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

void UCodexPanelWidget::PopulateIndustryRail()
{
	RailIndustries.Reset();
	OwnedIndustries.Reset();
	// 풀링된 인스턴스가 재사용되면 NativeConstruct 가 다시 도니, 레일을 새로 짤 때 적용 상태도 같이 되돌린다
	bIndustryApplied = false;
	if (!IndustryRailBox)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CodexPanel] IndustryRailBox 미배치 ― 산업 레일이 뜨지 않습니다(WBP 트리 저작 전)"));
		return;
	}
	IndustryRailBox->ClearChildren();

	UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance());
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	TSubclassOf<UUserWidget> TabClass = TableMgr ? TableMgr->GetWidgetClass(EWidgetType::IndustryRailTab) : nullptr;
	if (!TabClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CodexPanel] IndustryRailTab 위젯 미등록(DT_WidgetClass)"));
		return;
	}

	USaveGame_GameData* SaveData = nullptr;
	if (GI)
	{
		if (USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>())
		{
			SaveData = SaveMgr->GetCurrentSaveData();
		}
	}

	TArray<FCodexIndustrySummary> Rows;
	if (SaveData) { CodexSummary::BuildAllIndustrySummaries(SaveData->GameData, Rows); }

	// 집계는 6산업을 그대로 낸다 ― BuildAllIndustrySummaries 는 FGameSaveData 만 받는 월드 비의존
	// 순수 함수라 DT 가용성을 물을 수 없고(테스트가 월드 없이 돈다), 표시 여부는 UI 가 정한다.
	if (USaveLoadManager* SaveMgr = GI ? GI->GetSubsystem<USaveLoadManager>() : nullptr)
	{
		Rows.RemoveAll([SaveMgr](const FCodexIndustrySummary& Row)
		{
			return !SaveMgr->IsIndustryAvailable(Row.Industry);
		});
	}

	RailGroup = NewObject<UCommonButtonGroupBase>(this);
	RailGroup->SetSelectionRequired(true);

	for (const FCodexIndustrySummary& Row : Rows)
	{
		UIconWithButtonWidget* Tab = CreateWidget<UIconWithButtonWidget>(this, TabClass);
		if (!Tab) { continue; }

		// 값 주입 전에 붙인다 ― AddChild 가 Slate 를 세우며 NativePreConstruct 를 돌리고,
		// 거기서 SetCount(Count=0) 가 CountText 를 Collapsed 로 되돌려 보조 줄이 통째로 사라진다
		IndustryRailBox->AddChild(Tab);

		bool bFound = false;
		const FCompanyInfoTable Info = TableMgr->GetCompanyInfo(Row.Industry, bFound);
		if (bFound)
		{
			Tab->SetButtonText(Info.DisplayName);
			// 레일은 분야 비교표라 색이 가장 빠른 식별 신호다 ― 흰 웰 채움에 산업색을 곱한다
			Tab->SetIconWellColor(Info.AccentColor);
			if (UTexture2D* Glyph = Info.GlyphIcon.IsNull() ? nullptr : Info.GlyphIcon.LoadSynchronous())
			{
				Tab->SetIcon(Glyph);
			}
		}

		if (Row.bOwned)
		{
			// 보조 한 줄 = 진척 단계 · 누적 수익 (스펙 치수 절)
			Tab->SetCountText(FText::Format(
				NSLOCTEXT("Codex", "RailSub", "{0}단계 · {1}"),
				FText::AsNumber(Row.Tier),
				UGlobalUtilFunctions::AbbreviateNumber(Row.CumulativeRevenue)));
			OwnedIndustries.Add(Row.Industry);
		}
		else
		{
			Tab->SetCountText(NSLOCTEXT("Codex", "RailLocked", "미진출"));
		}

		// 누를 수는 있되 "아직 안 간 곳" 위계는 남긴다. 상호작용을 끄면 죽은 탭이 되고,
		// 아무 표시도 안 하면 보유 산업과 구별이 '미진출' 네 글자뿐이다.
		// RenderOpacity 는 그리기만 줄여 자리·크기·히트테스트를 건드리지 않는다(레퍼런스 §1.1-21).
		// 양쪽을 다 지정한다 ― 풀에서 재사용된 탭이 흐린 채로 돌아오면 안 된다.
		Tab->SetRenderOpacity(Row.bOwned ? 1.0f : 0.55f);

		RailIndustries.Add(Row.Industry);
		RailGroup->AddWidget(Tab);
		// R2 — 미진출도 열린다. 도감은 원래 빈 칸을 보여주는 물건이라, 0 으로 채운 같은 판을 연다.
		// bIsSelectable 과 IsInteractionEnabled 는 별개다 ― 후자가 꺼져 있으면 클릭이 아예 안 들어와
		// 전자를 켜도 소용없다. 둘 다 켜야 죽은 탭이 풀린다.
		Tab->SetIsSelectable(true);
		Tab->SetIsInteractionEnabled(true);
	}

	RailGroup->OnSelectedButtonBaseChanged.AddDynamic(this, &UCodexPanelWidget::OnIndustryTabChanged);
}

void UCodexPanelWidget::SelectIndustry(ECompanyType InIndustry)
{
	Industry = InIndustry;
	bIndustryApplied = true;

	// 티어·매출·출시작 수 = 그 산업에서 가장 앞선 건물. 레일이 이미 계산한 값을 다시 쓴다.
	// 미진출은 Tier 0 으로 남긴다 — 1 로 채우면 진출한 적 없는 산업이 "TIER 1"으로 표시된다.
	CurrentTier = 0;
	bIndustryOwned = false;
	IndustryRevenue = 0;
	IndustryShippedCount = 0;
	if (UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance()))
	{
		if (USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>())
		{
			if (USaveGame_GameData* SaveData = SaveMgr->GetCurrentSaveData())
			{
				TArray<FCodexIndustrySummary> Rows;
				CodexSummary::BuildAllIndustrySummaries(SaveData->GameData, Rows);
				for (const FCodexIndustrySummary& Row : Rows)
				{
					if (Row.Industry != Industry) { continue; }
					bIndustryOwned = Row.bOwned;
					CurrentTier = Row.Tier;
					IndustryRevenue = Row.CumulativeRevenue;
					IndustryShippedCount = Row.ShippedCount;
				}
			}
		}
	}

	// 선택된 탭은 미진출이라도 또렷해야 한다 ― 흐린 채로 선택되면 "선택이 안 먹었다"로 읽힌다.
	// 흐리기는 "아직 안 간 곳" 표시지, "못 누르는 곳" 표시가 아니다.
	if (IndustryRailBox)
	{
		const TArray<UWidget*> RailChildren = IndustryRailBox->GetAllChildren();
		for (int32 TabIndex = 0; TabIndex < RailChildren.Num(); ++TabIndex)
		{
			if (!RailChildren[TabIndex] || !RailIndustries.IsValidIndex(TabIndex)) { continue; }
			const ECompanyType RowIndustry = RailIndustries[TabIndex];
			const bool bBright = (RowIndustry == Industry) || OwnedIndustries.Contains(RowIndustry);
			RailChildren[TabIndex]->SetRenderOpacity(bBright ? 1.0f : 0.55f);
		}
	}

	ApplyCompanyIdentity();
	PopulateRoadmap();
	PopulateTopHits();
	PopulateComboGrid();      // 발견/전체 카운트를 채운다
	RefreshIndustryRecord();  // 그 카운트를 읽으므로 그리드 뒤여야 한다
	PopulateShipped();
}

void UCodexPanelWidget::OnIndustryTabChanged(UCommonButtonBase* /*SelectedButton*/, int32 ButtonIndex)
{
	if (!RailIndustries.IsValidIndex(ButtonIndex)) { return; }
	SelectIndustry(RailIndustries[ButtonIndex]);
}

void UCodexPanelWidget::ApplyCompanyIdentity()
{
	UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance());
	if (!GI) { return; }

	if (UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>())
	{
		bool bFound = false;
		const FCompanyInfoTable Info = TableMgr->GetCompanyInfo(Industry, bFound);
		if (bFound) { IndustryColor = Info.AccentColor; }
	}

	if (CompanyPlate) { CompanyPlate->SetColorAndOpacity(IndustryColor); }
}

void UCodexPanelWidget::RefreshIndustryRecord()
{
	// R2 — 표제는 티어 사다리가 아니라 이 산업의 총괄 기록이다.
	// WBP 수술 없이 기존 required 텍스트 5개를 기록 4항 + 진척 1줄로 재사용한다.
	const bool bAllFound = (TotalComboCount > 0 && DiscoveredCount >= TotalComboCount);

	if (Text_RankTier)
	{
		Text_RankTier->SetText(FText::Format(NSLOCTEXT("Codex", "RecDiscovered", "{0} / {1}"),
			FText::AsNumber(DiscoveredCount), FText::AsNumber(TotalComboCount)));
		Text_RankTier->SetColorAndOpacity(FSlateColor(bAllFound
			? FLinearColor(0.498f, 0.839f, 0.627f, 1.f) : Lighten(IndustryColor, 0.35f)));
	}
	if (Text_RankTitle)
	{
		Text_RankTitle->SetText(NSLOCTEXT("Codex", "RecDiscoveredLabel", "발견한 조합"));
	}

	// R2-b ― 세 줄로 흩어져 있던 기록을 한 줄로 모은다. 흩어진 만큼 조합 그리드가 밀렸다.
	// 미진출이면 0 이 그대로 나오는 게 맞다(도감은 빈 칸을 보여주는 물건이다).
	FText BestChunk = FText::GetEmpty();
	if (UCGGameInstance* BestGI = UCGGameInstance::GetInstance())
	{
		if (USaveLoadManager* SaveMgr = BestGI->GetSubsystem<USaveLoadManager>())
		{
			if (USaveGame_GameData* SaveData = SaveMgr->GetCurrentSaveData())
			{
				int32 TopScore = -1;
				for (const FShippedProjectRecord& R : SaveData->GameData.ShippedProjects)
				{
					if (R.Industry == Industry && R.ReviewScore > TopScore) { TopScore = R.ReviewScore; }
				}
				if (TopScore >= 0)
				{
					BestChunk = FText::Format(NSLOCTEXT("Codex", "RecBestChunk", " · 최고평점 {0}/40"),
						FText::AsNumber(TopScore));
				}
			}
		}
	}

	if (Text_TierRank)
	{
		Text_TierRank->SetText(FText::Format(
			NSLOCTEXT("Codex", "RecOneLine", "출시작 {0}편 · 누적매출 {1}{2} · 진척 {3}/{4} 단계"),
			FText::AsNumber(IndustryShippedCount),
			UGlobalUtilFunctions::AbbreviateNumber(IndustryRevenue),
			BestChunk,
			FText::AsNumber(CurrentTier), FText::AsNumber(TierConstants::MAX_TIER)));
	}
	// 진척은 위 한 줄에 합쳐졌다 ― 따로 띄우면 같은 말을 두 번 한다
	if (Text_TierProgress) { Text_TierProgress->SetVisibility(ESlateVisibility::Collapsed); }

	// 안내 전용 줄 ― 최고평점은 위 한 줄에 들어갔으므로 여기선 "기록이 없다"만 말한다.
	// 말할 게 없으면 접는다: 빈 줄이 그리드 자리를 먹으면 안 된다.
	if (Text_TierReward)
	{
		const bool bHasRecord = IndustryShippedCount > 0;
		if (bHasRecord)
		{
			Text_TierReward->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			Text_TierReward->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			Text_TierReward->SetText(bIndustryOwned
				? NSLOCTEXT("Codex", "RecNoShip", "아직 출시한 작품이 없습니다")
				: NSLOCTEXT("Codex", "RecNotEntered", "아직 진출하지 않았습니다 ― 도시에서 이 업종 회사를 세우면 기록이 쌓입니다"));
		}
	}
}

void UCodexPanelWidget::PopulateRoadmap()
{
	// 진척 채움 라인 — 바로 아래 한 줄이 "진척 N/10 티어" 라고 말하므로 바도 같은 눈금이어야 한다.
	// 구 공식 (Tier-1)/(MAX-1) 은 10칸 사다리 시절 것이다(1칸=출발점). 사다리를 걷어낸 지금
	// 그대로 두면 5/10 인데 바는 44% 를 그려, 붙어 있는 두 요소가 서로 다른 말을 한다.
	if (RoadmapFillLine)
	{
		const float Frac = (TierConstants::MAX_TIER > 0)
			? FMath::Clamp(float(CurrentTier) / float(TierConstants::MAX_TIER), 0.f, 1.f) : 0.f;
		FWidgetTransform FillT;
		FillT.Scale = FVector2D(Frac, 1.f);
		RoadmapFillLine->SetRenderTransform(FillT);
		RoadmapFillLine->SetColorAndOpacity(Lighten(IndustryColor, 0.12f));
	}

	// R2 — 10칸 티어 사다리를 걷어낸다. 그건 진행도 화면(기획 보드)의 언어고, 도감은 기록이다.
	// 티어는 채움 라인 + 아래 진척 한 줄로만 남는다.
	if (RoadmapBox)
	{
		RoadmapBox->ClearChildren();
		RoadmapBox->SetVisibility(ESlateVisibility::Collapsed);
	}

	// 진척 문구는 RefreshIndustryRecord 의 한 줄 요약이 맡는다(R2-b). 여기선 채움 라인만 그린다.
}

void UCodexPanelWidget::PopulateTopHits()
{
	// R2-b ― 대표작 3칸을 [프로젝트] 탭에서 걷어냈다. 세로 1192px 중 300px(25%)을 먹으면서
	// 정작 도감 본체인 조합 그리드는 230px(19%)밖에 못 받고 있었다(2026-09-23 캐처 실측).
	// 같은 데이터(누적매출 상위 3)를 [출시작] 탭이 매출순 정렬로 이미 보여주고,
	// 잃는 그림은 그쪽 행에 커버 썸네일을 넣어 메졌다. WBP 위젯은 남기고 접기만 한다.
	if (TopHitsSection) { TopHitsSection->SetVisibility(ESlateVisibility::Collapsed); }
	if (TopHitsBox) { TopHitsBox->ClearChildren(); }
}

void UCodexPanelWidget::ResolveDevelopedProjects(TArray<int32>& OutDeveloped) const
{
	OutDeveloped.Reset();
	if (UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance()))
	{
		if (USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>())
		{
			if (USaveGame_GameData* SaveData = SaveMgr->GetCurrentSaveData())
			{
				CodexSummary::BuildClearedProjects(SaveData->GameData, Industry, OutDeveloped);
			}
		}
	}
}

void UCodexPanelWidget::PopulateComboGrid()
{
	DiscoveredCount = 0;
	TotalComboCount = 0;
	if (!DexGroupsBox) { return; }
	DexGroupsBox->ClearChildren();

	UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	TSubclassOf<UUserWidget> CellClass = TableMgr ? TableMgr->GetWidgetClass(EWidgetType::PortfolioCell) : nullptr;
	if (!TableMgr || !CellClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CodexPanel] PortfolioCell/TableMgr 미준비"));
		return;
	}

	// R2 — 전 티어를 한 판에 모아 장르 그룹으로 펼친다. 도감은 "이 산업에 무엇이 있고 무엇을 찾았나"이지
	// 티어별 진도표가 아니다. 착수는 기획 보드 담당(여긴 표시만).
	TArray<int32> Developed;
	ResolveDevelopedProjects(Developed);
	TArray<FProjectTierEntry> Entries;
	for (int32 TierIndex = 1; TierIndex <= TierConstants::MAX_TIER; ++TierIndex)
	{
		TArray<FProjectTierEntry> TierEntries;
		UOfficeStageProgressManager::BuildTierProjectEntries(Industry, TierIndex, CurrentTier, Developed, TierEntries);
		Entries.Append(MoveTemp(TierEntries));
	}

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
			const FShippedProjectRecord* Rec = nullptr;
			if (Shipped)
			{
				for (const FShippedProjectRecord& Candidate : *Shipped)
				{
					if (Candidate.ProjectIndex == E.ProjectIndex && Candidate.Industry == Industry
						&& (!Rec || static_cast<uint8>(Candidate.QualityGrade) > static_cast<uint8>(Rec->QualityGrade)))
					{
						Rec = &Candidate;
					}
				}
			}
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

	// 표제(기록 요약)가 읽어가도록 멤버로 남긴다 — Text_TierProgress 는 이제 진척 줄이라 여기서 안 건드린다
	DiscoveredCount = DevelopedCount;
	TotalComboCount = TotalCount;
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
		// 크림 카드 위라 다크 토큰(#8A97A8)이 아니라 #C4B08E
		UVerticalBox* EmptyCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UImage* EmptyGlyph = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		if (UVerticalBoxSlot* GlyphSlot = EmptyCol->AddChildToVerticalBox(EmptyGlyph))
		{
			GlyphSlot->SetHorizontalAlignment(HAlign_Center);
		}
		UGlobalUtilFunctions::ApplyWidgetIcon(EmptyGlyph,
			TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/CompanyGrowth/UI/Textures/UIIcon/Empty/T_UIIcon_Empty_Book.T_UIIcon_Empty_Book"))),
			FVector2D(96.f, 96.f), FMargin(0.f, 0.f, 0.f, 12.f));
		EmptyGlyph->SetColorAndOpacity(FLinearColor(0.553f, 0.434f, 0.271f, 1.0f));
		if (UVerticalBoxSlot* MsgSlot = EmptyCol->AddChildToVerticalBox(
			MakeText(FText::FromString(TEXT("아직 출시한 작품이 없습니다")), 24, InkSub, false)))
		{
			MsgSlot->SetHorizontalAlignment(HAlign_Center);
		}
		EmptyCard->SetContent(EmptyCol);
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
		// MinDesired 로 둔다 ― WidthOverride 는 상한이자 하한이라 3자리 순위(100위~)가 잘린다.
		USizeBox* RankBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		RankBox->SetMinDesiredWidth(58.f);
		RankBox->SetMinDesiredHeight(58.f);
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

		// 커버 썸네일 ― [프로젝트] 탭의 대표작 3칸을 걷어내면서 그림을 여기로 옮겼다.
		// 같은 데이터(누적매출 상위)가 두 탭에 있었고, 큰 쪽이 도감 본체인 조합 그리드를 밀어내고 있었다.
		UTexture2D* RowCover = nullptr;
		if (UTableManagerSubsystem* CoverMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr)
		{
			bool bCoverFound = false;
			const FProjectData Proj = CoverMgr->GetProjectData(Industry, Rec.ProjectIndex, bCoverFound);
			if (bCoverFound) { RowCover = Proj.Icon.LoadSynchronous(); }
		}
		if (RowCover)
		{
			USizeBox* CoverBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			CoverBox->SetMinDesiredWidth(96.f);
			CoverBox->SetMinDesiredHeight(72.f);
			UImage* CoverImg = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			CoverImg->SetBrushFromTexture(RowCover, false);
			CoverBox->SetContent(CoverImg);
			if (UHorizontalBoxSlot* CoverSlot = RowBox->AddChildToHorizontalBox(CoverBox))
			{
				CoverSlot->SetPadding(FMargin(0.f, 0.f, 18.f, 0.f));
				CoverSlot->SetVerticalAlignment(VAlign_Center);
			}
		}

		// 메인: 이름(크게, 잉크) + 메타 pill(장르·소재 / 등급)
		UVerticalBox* Main = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		if (UCommonTextBlock* NameText = MakeText(FText::FromString(Rec.ProjectName), 28, Ink, true))
		{
			// 생략 정책이 없으면 긴 이름이 비례 압축에 눌려 글자 중간에서 잘린다(= "깨짐").
			// TextBlock 의 desired size 는 줄바꿈 안 한 전체 폭이라 Fill 슬롯만으로는 못 막는다.
			NameText->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
			NameText->SetClipping(EWidgetClipping::ClipToBounds);
			Main->AddChildToVerticalBox(NameText);
		}

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
		ScoreBox->SetMinDesiredWidth(120.f);
		ScoreBox->SetMinDesiredHeight(72.f);
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
