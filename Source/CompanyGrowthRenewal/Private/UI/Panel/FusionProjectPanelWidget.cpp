#include "UI/Panel/FusionProjectPanelWidget.h"
#include "CommonTextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Global/GlobalUtilFunctions.h"
#include "Data/EmployeeTypes.h"
#include "Data/GameClockRules.h"
#include "Enum/WidgetType.h"
#include "Manager/EmployeeManager.h"
#include "Manager/SideContentSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/UIManagerSubsystem.h"
#include "Table/CompanyInfoTable.h"
#include "Table/FusionProjectTable.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Common/RewardChipUtils.h"
#include "UI/Element/Common/EmptyStateWidget.h"
#include "UI/Element/Fusion/FusionCardWidget.h"
#include "UI/Element/Fusion/FusionSlotWidget.h"
#include "UI/Element/Fusion/FusionBuildingRowWidget.h"
#include "UI/Element/SideContent/BusinessTripCardWidget.h"

USideContentSubsystem* UFusionProjectPanelWidget::Side() const
{
	return GetGameInstance() ? GetGameInstance()->GetSubsystem<USideContentSubsystem>() : nullptr;
}

UTableManagerSubsystem* UFusionProjectPanelWidget::Tables() const
{
	return GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
}

void UFusionProjectPanelWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	GridSizer.Bind(BoardGrid, nullptr);
}

int32 UFusionProjectPanelWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	// 배경 자신의 레이아웃 크기로 반복 간격을 유지한다. DPI와 경고 띠가 바뀌어도 셀은 정사각형이다.
	GridSizer.Update();
	return Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
}

void UFusionProjectPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (StartButton) StartButton->OnClicked().AddUObject(this, &UFusionProjectPanelWidget::HandleStart);
	if (ClaimButton) ClaimButton->OnClicked().AddUObject(this, &UFusionProjectPanelWidget::HandleClaim);
}

void UFusionProjectPanelWidget::NativeDestruct()
{
	StopClock();
	if (StartButton) StartButton->OnClicked().RemoveAll(this);
	if (ClaimButton) ClaimButton->OnClicked().RemoveAll(this);
	Super::NativeDestruct();
}

void UFusionProjectPanelWidget::NativeOnActivated()
{
	USideContentSubsystem* System = Side();
	UTableManagerSubsystem* TableManager = Tables();
	if (System)
	{
		System->OnSideContentChanged.RemoveAll(this);
		System->OnSideContentChanged.AddUObject(this, &UFusionProjectPanelWidget::RefreshAll);
	}
	if (UEmployeeManager* Employees = GetGameInstance() ? GetGameInstance()->GetSubsystem<UEmployeeManager>() : nullptr)
	{
		Employees->OnEmployeeStatsChanged.AddUObject(this, &UFusionProjectPanelWidget::HandleStatsChanged);
		Employees->OnEmployeeRosterChanged.AddUObject(this, &UFusionProjectPanelWidget::RefreshAll);
	}
	// 3열 보드에는 목록 화면이 없다 — 진행 중인 콜라보, 없으면 가용 목록 첫 행을 미리 골라 둔다.
	// 진행 중인 키는 미가용이어도 그대로 연다 — 수령해서 끝낼 경로를 남겨야 세이브가 자기치유된다.
	if (System && TableManager)
	{
		const TArray<FName> Keys = System->GetAvailableFusionKeys();
		FName InitialKey = System->IsFusionActive() ? System->GetSideContentData().FusionActiveKey : NAME_None;
		if (InitialKey.IsNone() && !Keys.IsEmpty()) InitialKey = Keys[0];
		SelectCollab(InitialKey);
	}
	Super::NativeOnActivated();
	if (UWorld* WidgetWorld = GetWorld())
	{
		// 시간 표시만 갱신한다. 목록·편성 데이터는 변경 이벤트를 받는다.
		WidgetWorld->GetTimerManager().SetTimer(ClockHandle, this, &UFusionProjectPanelWidget::RefreshClock, 1.f, true);
	}
}

void UFusionProjectPanelWidget::NativeOnDeactivated()
{
	StopClock();
	if (USideContentSubsystem* System = Side()) System->OnSideContentChanged.RemoveAll(this);
	if (UEmployeeManager* Employees = GetGameInstance() ? GetGameInstance()->GetSubsystem<UEmployeeManager>() : nullptr)
	{
		Employees->OnEmployeeStatsChanged.RemoveAll(this);
		Employees->OnEmployeeRosterChanged.RemoveAll(this);
	}
	Super::NativeOnDeactivated();
}

void UFusionProjectPanelWidget::StopClock()
{
	if (UWorld* WidgetWorld = GetWorld()) WidgetWorld->GetTimerManager().ClearTimer(ClockHandle);
}

void UFusionProjectPanelWidget::HandleStatsChanged(int32 EmployeeID)
{
	// 스탯은 팀 파워만 움직인다 — 레일의 보유 업종 판정은 그대로라 다시 세우지 않는다.
	RefreshSlots();
	RefreshCandidates();
	RefreshDetail();
}

void UFusionProjectPanelWidget::RefreshAll()
{
	RefreshBoard();
}

void UFusionProjectPanelWidget::RefreshBoard()
{
	USideContentSubsystem* System = Side();
	UTableManagerSubsystem* TableManager = Tables();
	if (!System || !TableManager) return;
	RefreshRail();
	RefreshSlots();
	RefreshCandidates();
	RefreshDetail();
	// DT 미임포트/로드 실패는 빈 판이 아니라 문구로 드러나야 한다(구 ListHintText 가 하던 일).
	// 가용 업종 필터가 전부 걸러낸 경우도 같은 자리 — 어느 쪽이든 고를 카드가 없다. 로그로 갈린다.
	if (System->GetAvailableFusionKeys().IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Fusion] 올릴 콜라보가 없습니다 (DT 행 %d개, 가용 0개) — 임포트 누락 또는 업종 전부 미가용"),
			TableManager->GetFusionProjectOrder().Num());
		const FText Message = BlockReason(EFusionBlockReason::NoProject);
		if (BlockMessageText) BlockMessageText->SetText(Message);
		// RefreshDetail 이 조기 리턴해 CTA 가 저작 기본값(보이고 활성)으로 남는다 — 안내 아래 죽은 버튼을 접는다.
		if (StartButton) StartButton->SetVisibility(ESlateVisibility::Collapsed);
		if (EmptyBuildings)
		{
			EmptyBuildings->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			EmptyBuildings->SetMessage(Message);
		}
	}
}

void UFusionProjectPanelWidget::RefreshRail()
{
	USideContentSubsystem* System = Side();
	UTableManagerSubsystem* TableManager = Tables();
	if (!System) return;
	// 레일이 아직 없어도(구 WBP) 래치는 맞춰 둔다 — 안 그러면 RefreshClock 이 매초 레일을 다시 세운다.
	bLastClaimable = System->IsFusionClaimable();
	if (!TableManager || !CollabRail) return;
	// 순서가 움직이면 오히려 찾기 어렵다 — DT 순서를 그대로 쓴다. 미보유는 상태줄이 이미 구분한다.
	// 목록 길이는 고정이 아니다: 미가용 업종(이번 빌드 범위 밖)을 요구하는 행은 아예 빠진다.
	const TArray<FName> Keys = System->GetAvailableFusionKeys();
	const FName ActiveKey = System->GetSideContentData().FusionActiveKey;
	if (Keys != ListedKeys || Cards.Num() != Keys.Num())
	{
		CollabRail->ClearChildren();
		Cards.Reset();
		ListedKeys = Keys;
		const TSubclassOf<UUserWidget> CardClass = TableManager->GetWidgetClass(EWidgetType::FusionCard);
		if (!CardClass) return;
		for (int32 CardIndex = 0; CardIndex < Keys.Num(); ++CardIndex)
		{
			UFusionCardWidget* Card = CreateWidget<UFusionCardWidget>(GetOwningPlayer(), CardClass);
			if (!Card) continue;
			if (UVerticalBoxSlot* BoxSlot = CollabRail->AddChildToVerticalBox(Card))
				BoxSlot->SetPadding(FMargin(0.f, 0.f, 0.f, CardIndex == Keys.Num() - 1 ? 0.f : RailSpacing));
			Card->OnFusionClicked.AddUObject(this, &UFusionProjectPanelWidget::HandleCard);
			Cards.Add(Card);
		}
	}
	for (int32 CardIndex = 0; CardIndex < Cards.Num() && CardIndex < Keys.Num(); ++CardIndex)
	{
		FFusionProjectTable Row;
		if (!TableManager->GetFusionProject(Keys[CardIndex], Row)) continue;
		const bool bActive = ActiveKey == Keys[CardIndex];
		const bool bSelected = SelectedKey == Keys[CardIndex];
		const ECompanyType Missing = System->GetFusionMissingIndustry(Keys[CardIndex]);
		FText Status = NSLOCTEXT("Fusion", "SelectToDevelop", "선택하여 개발");
		if (bActive) Status = System->IsFusionClaimable() ? NSLOCTEXT("Fusion", "ClaimReady", "수령 가능") : NSLOCTEXT("Fusion", "InProgress", "진행 중");
		else if (Missing != ECompanyType::None) Status = MissingIndustryText(Missing);
		else if (bSelected) Status = NSLOCTEXT("Fusion", "SelectedNow", "선택 중");
		Cards[CardIndex]->Configure(Keys[CardIndex], Row, System->GetFusionLevel(Keys[CardIndex]),
			System->GetFusionRequirement(Keys[CardIndex]), System->GetFusionRewardPreview(Keys[CardIndex]),
			Status, Missing != ECompanyType::None, bSelected);
	}
}

void UFusionProjectPanelWidget::RefreshSlots()
{
	USideContentSubsystem* System = Side();
	UTableManagerSubsystem* TableManager = Tables();
	if (!System || !TableManager || !SlotBox) return;
	if (Slots.Num() != SlotIndustries.Num())
	{
		SlotBox->ClearChildren();
		Slots.Reset();
		const TSubclassOf<UUserWidget> SlotClass = TableManager->GetWidgetClass(EWidgetType::FusionSlot);
		if (!SlotClass) return;
		for (int32 SlotIndex = 0; SlotIndex < SlotIndustries.Num(); ++SlotIndex)
		{
			UFusionSlotWidget* SlotWidget = CreateWidget<UFusionSlotWidget>(GetOwningPlayer(), SlotClass);
			if (!SlotWidget) continue;
			if (UHorizontalBoxSlot* BoxSlot = SlotBox->AddChildToHorizontalBox(SlotWidget))
			{
				BoxSlot->SetPadding(FMargin(SlotIndex == 0 ? 0.f : SlotSpacing, 0.f, 0.f, 0.f));
				BoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			}
			Slots.Add(SlotWidget);
		}
	}
	const float Requirement = System->GetFusionRequirement(SelectedKey);
	for (int32 SlotIndex = 0; SlotIndex < Slots.Num() && SlotIndex < SlotIndustries.Num(); ++SlotIndex)
	{
		const int32 BuildingIndex = PickedBuildings.IsValidIndex(SlotIndex) ? PickedBuildings[SlotIndex] : INDEX_NONE;
		// 타일 클릭이 산업으로 슬롯을 정하므로 슬롯은 편집 대상이 아니다 — 포커스 링을 켜지 않는다.
		Slots[SlotIndex]->Configure(SlotIndex, SlotIndustries[SlotIndex], BuildingIndex,
			BuildingIndex != INDEX_NONE ? System->GetTrainingTeamPower(BuildingIndex) : 0.f,
			Requirement, false,
			System->GetFusionCandidateBuildings(SlotIndustries[SlotIndex]).IsEmpty());
	}
}

void UFusionProjectPanelWidget::RefreshCandidates()
{
	USideContentSubsystem* System = Side();
	UTableManagerSubsystem* TableManager = Tables();
	if (!System || !TableManager || !CandidateGrid) return;

	BuildingToSlot.Reset();
	ListedBuildings.Reset();
	for (int32 SlotIndex = 0; SlotIndex < SlotIndustries.Num(); ++SlotIndex)
	{
		// 콜라보 행 전부가 슬롯별 산업이 달라 합집합에 중복이 생기지 않는다.
		for (int32 BuildingIndex : System->GetFusionCandidateBuildings(SlotIndustries[SlotIndex]))
		{
			BuildingToSlot.Add(BuildingIndex, SlotIndex);
			ListedBuildings.Add(BuildingIndex);
		}
	}

	if (CandidateHintText)
	{
		TArray<FString> Names;
		for (ECompanyType Industry : SlotIndustries)
		{
			bool bFound = false;
			const FCompanyInfoTable Info = TableManager->GetCompanyInfo(Industry, bFound);
			if (bFound) Names.Add(Info.DisplayName.ToString());
		}
		CandidateHintText->SetText(IsRunning(SelectedKey)
			? NSLOCTEXT("Fusion", "CandidateLocked", "진행 중에는 바꿀 수 없습니다")
			: FText::Format(NSLOCTEXT("Fusion", "CandidateHint", "{0} 업종만 배치할 수 있습니다"),
				FText::FromString(FString::Join(Names, TEXT(" · ")))));
	}

	// 후보가 전부 비었을 때만 안내를 띄운다. 부분 미보유면 남은 업종 타일은 그대로 보여야 한다.
	if (EmptyBuildings)
	{
		EmptyBuildings->SetVisibility(ListedBuildings.IsEmpty()
			? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		EmptyBuildings->SetMessage(MissingIndustryText(System->GetFusionMissingIndustry(SelectedKey)));
	}
	// 빈 그리드를 남기면 안내와 자리를 나눠 갖는다 — 캡처와 같게 자리까지 회수한다.
	CandidateGrid->SetVisibility(ListedBuildings.IsEmpty()
		? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);

	CandidateGrid->ClearChildren();
	BuildingRows.Reset();
	const TSubclassOf<UUserWidget> TileClass = TableManager->GetWidgetClass(EWidgetType::FusionBuildingRow);
	if (!TileClass) return;
	UEmployeeManager* Employees = GetGameInstance() ? GetGameInstance()->GetSubsystem<UEmployeeManager>() : nullptr;
	const float Requirement = System->GetFusionRequirement(SelectedKey);
	const bool bLocked = IsRunning(SelectedKey);
	// SetIsInteractionEnabled(false) 는 겉모습을 안 바꾼다 — 불투명도를 같이 낮춰야 잠긴 줄 보인다.
	// 렌더 불투명도는 레이아웃에 영향이 없어 기하 검사를 흔들지 않는다.
	if (CandidateGridScroll) CandidateGridScroll->SetRenderOpacity(bLocked ? .45f : 1.f);
	for (int32 Index = 0; Index < ListedBuildings.Num(); ++Index)
	{
		const int32 BuildingIndex = ListedBuildings[Index];
		UFusionBuildingRowWidget* Tile = CreateWidget<UFusionBuildingRowWidget>(GetOwningPlayer(), TileClass);
		if (!Tile) continue;
		if (UUniformGridSlot* GridSlot = CandidateGrid->AddChildToUniformGrid(Tile, Index / 4, Index % 4))
		{
			GridSlot->SetHorizontalAlignment(HAlign_Fill);
			GridSlot->SetVerticalAlignment(VAlign_Top);
		}
		const int32 SlotIndex = BuildingToSlot[BuildingIndex];
		Tile->Configure(BuildingIndex, SlotIndustries[SlotIndex], System->GetTrainingTeamPower(BuildingIndex),
			Employees ? Employees->GetEmployeesInBuilding(BuildingIndex).Num() : 0,
			Requirement, PickedBuildings.IsValidIndex(SlotIndex) && PickedBuildings[SlotIndex] == BuildingIndex,
			false);
		// Configure 가 인원·중복으로 이미 닫은 타일을 되열지 않도록 잠금만 덧씌운다.
		if (bLocked) Tile->SetIsInteractionEnabled(false);
		Tile->OnFusionBuildingClicked.AddUObject(this, &UFusionProjectPanelWidget::HandleBuilding);
		BuildingRows.Add(Tile);
	}
}

void UFusionProjectPanelWidget::RefreshDetail()
{
	USideContentSubsystem* System = Side();
	UTableManagerSubsystem* TableManager = Tables();
	FFusionProjectTable Row;
	if (!System || !TableManager || !TableManager->GetFusionProject(SelectedKey, Row)) return;

	const bool bThisRunning = IsRunning(SelectedKey);
	const float Requirement = System->GetFusionRequirement(SelectedKey);

	RefreshCover(Row.RequiredIndustries);
	if (DetailNameText) DetailNameText->SetText(Row.DisplayName);
	if (DetailIndustryText)
	{
		TArray<FString> Names;
		for (ECompanyType Industry : Row.RequiredIndustries)
		{
			bool bFound = false;
			const FCompanyInfoTable Info = TableManager->GetCompanyInfo(Industry, bFound);
			if (bFound) Names.Add(Info.DisplayName.ToString());
		}
		DetailIndustryText->SetText(FText::FromString(FString::Join(Names, TEXT(" + "))));
	}
	// 수령으로 레벨이 오르면 요구치가 x1.12 된다 — 라벨과 파워 열이 같은 Requirement 를 읽어야 한다.
	// 포매터 고정: 렌더 픽스처가 AsNumber(CeilToInt(...)) 로 그리므로 여기도 같아야 한다.
	// AbbreviateNumber 를 쓰면 캡처가 제품이 안 그리는 문자열을 담는다. 단 UIE_FusionSlot 은 파워를
	// AbbreviateNumber 로 그리므로 요구치가 10,000 을 넘는 시점(대략 Lv.26)부터 슬롯 "1만" 과
	// 라벨 "10,000" 으로 갈린다 — 그때 양쪽을 한 포매터로 통일할 것.
	if (PowerLabelText) PowerLabelText->SetText(FText::Format(
		NSLOCTEXT("Fusion", "PowerLabel", "현재 팀 파워 (요구 {0})"), FText::AsNumber(FMath::CeilToInt(Requirement))));

	if (InfoWell) InfoWell->SetVisibility(bThisRunning ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	if (RunWell) RunWell->SetVisibility(bThisRunning ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (DurationText) DurationText->SetText(UBusinessTripCardWidget::FormatTripDuration(Row.DurationSeconds));
	// 시가총액 수치는 보상 칩과 같은 배열에서 뽑는다 — 레벨 곡선이 바뀌어도 둘이 갈릴 수 없다.
	const TArray<FMissionReward> RewardPreview = System->GetFusionRewardPreview(SelectedKey);
	if (RewardBox && WidgetTree) CGRewardChip::RenderIcons(*WidgetTree, GetGameInstance(), *RewardBox, RewardPreview);
	if (MarketCapText)
	{
		const FMissionReward* MarketCap = RewardPreview.FindByPredicate([](const FMissionReward& Reward)
			{ return Reward.ResourceType == EResourceType::MarketCap; });
		MarketCapText->SetVisibility(MarketCap ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		if (MarketCap) MarketCapText->SetText(FText::Format(NSLOCTEXT("Fusion", "MarketCapGain", "+{0}"),
			UGlobalUtilFunctions::AbbreviateNumber(MarketCap->Amount, ENumberAbbrevStyle::Auto, ENumberRoundMode::Floor)));
	}
	if (RewardHintText) RewardHintText->SetText(FText::Format(
		NSLOCTEXT("Fusion", "RewardHint", "Lv.{0} → Lv.{1} 상승합니다"),
		FText::AsNumber(System->GetFusionLevel(SelectedKey)), FText::AsNumber(System->GetFusionLevel(SelectedKey) + 1)));

	const EFusionBlockReason Reason = System->GetFusionBlockReason(SelectedKey, PickedBuildings);
	if (BlockMessageText) BlockMessageText->SetText(bThisRunning ? FText::GetEmpty() : BlockReason(Reason));
	// 진행 중인 콜라보를 보는 동안만 수령 버튼이 나온다. 다른 콜라보를 열람하면 평시 화면 + ActiveProject 차단.
	if (StartButton)
	{
		StartButton->SetVisibility(bThisRunning ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
		StartButton->SetIsEnabled(Reason == EFusionBlockReason::None);
		StartButton->SetButtonText(NSLOCTEXT("Fusion", "StartCTA", "개발 시작"));
	}
	if (ClaimButton)
	{
		ClaimButton->SetVisibility(bThisRunning ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		const bool bReady = System->IsFusionClaimable();
		ClaimButton->SetIsEnabled(bReady);
		ClaimButton->SetButtonText(bReady ? NSLOCTEXT("Fusion", "ClaimCTA", "수령") : NSLOCTEXT("Fusion", "RunningCTA", "진행 중"));
	}
	RefreshPowerColumns(Requirement);
	RefreshClock();
}

void UFusionProjectPanelWidget::RefreshCover(const TArray<ECompanyType>& Industries)
{
	UTableManagerSubsystem* TableManager = Tables();
	if (!DetailCoverBox || !TableManager || !WidgetTree) return;
	// 저작된 샘플 자식의 브러시·패딩을 읽어 런타임 글리프에 그대로 입힌다 — 크기·틴트·드로우는 WBP 소유다
	// (선례 CGRewardChip::RenderIcons). ClearChildren 으로 샘플을 지우기 전에 먼저 측정해야 한다.
	FSlateBrush SampleBrush;
	FMargin SamplePadding(0.f);
	for (int32 ChildIndex = 0; ChildIndex < DetailCoverBox->GetChildrenCount(); ++ChildIndex)
	{
		if (const UImage* Sample = Cast<UImage>(DetailCoverBox->GetChildAt(ChildIndex)))
		{
			SampleBrush = Sample->GetBrush();
			if (const UHorizontalBoxSlot* SampleSlot = Cast<UHorizontalBoxSlot>(Sample->Slot)) SamplePadding = SampleSlot->GetPadding();
			break;
		}
	}
	DetailCoverBox->ClearChildren();
	for (ECompanyType Industry : Industries)
	{
		bool bFound = false;
		const FCompanyInfoTable Info = TableManager->GetCompanyInfo(Industry, bFound);
		UTexture2D* Glyph = bFound && !Info.GlyphIcon.IsNull() ? Info.GlyphIcon.LoadSynchronous() : nullptr;
		if (!Glyph) continue;
		UImage* Tile = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		FSlateBrush TileBrush = SampleBrush;
		TileBrush.SetResourceObject(Glyph);
		Tile->SetBrush(TileBrush);
		if (UHorizontalBoxSlot* BoxSlot = DetailCoverBox->AddChildToHorizontalBox(Tile))
		{
			BoxSlot->SetPadding(SamplePadding);
			BoxSlot->SetVerticalAlignment(VAlign_Center);
		}
	}
}

void UFusionProjectPanelWidget::RefreshPowerColumns(float Requirement)
{
	USideContentSubsystem* System = Side();
	UTableManagerSubsystem* TableManager = Tables();
	if (!System || !TableManager) return;
	UWidget* const Cols[3]           = { PowerCol0, PowerCol1, PowerCol2 };
	UCommonTextBlock* const Pcts[3]  = { PowerPct0, PowerPct1, PowerPct2 };
	UProgressBar* const Meters[3]    = { PowerMeter0, PowerMeter1, PowerMeter2 };
	UCommonTextBlock* const Names[3] = { PowerName0, PowerName1, PowerName2 };

	for (int32 Index = 0; Index < 3; ++Index)
	{
		if (!Cols[Index]) continue;
		// 슬롯이 2개인 콜라보에서 3번째 칸은 자리까지 회수한다(Hidden 이면 폭이 남는다).
		if (!SlotIndustries.IsValidIndex(Index)) { Cols[Index]->SetVisibility(ESlateVisibility::Collapsed); continue; }
		Cols[Index]->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

		const int32 BuildingIndex = PickedBuildings.IsValidIndex(Index) ? PickedBuildings[Index] : INDEX_NONE;
		const float Power = BuildingIndex != INDEX_NONE ? System->GetTrainingTeamPower(BuildingIndex) : 0.f;
		const float Ratio = Requirement > 0.f ? Power / Requirement : 0.f;
		if (Pcts[Index]) Pcts[Index]->SetText(BuildingIndex == INDEX_NONE
			? FText::FromString(TEXT("―"))   // U+2015 — NEXON 에 U+2014 글리프가 없다
			: FText::Format(NSLOCTEXT("Fusion", "PowerPct", "{0}%"), FText::AsNumber(FMath::RoundToInt(Ratio * 100.f))));
		if (Meters[Index]) Meters[Index]->SetPercent(FMath::Clamp(Ratio, 0.f, 1.f));
		if (Names[Index])
		{
			bool bFound = false;
			const FCompanyInfoTable Info = TableManager->GetCompanyInfo(SlotIndustries[Index], bFound);
			Names[Index]->SetText(bFound ? Info.DisplayName : FText::GetEmpty());
		}
	}
}

void UFusionProjectPanelWidget::RefreshClock()
{
	USideContentSubsystem* System = Side();
	if (!System) return;
	const bool bReady = System->IsFusionClaimable();
	// 레일 상태줄이 진행 중과 수령 가능으로 갈리는 순간만 다시 세운다. 보드 전체는 건드리지 않는다.
	if (bReady != bLastClaimable) RefreshRail();
	if (!IsRunning(SelectedKey)) return;
	const int32 Remaining = System->GetFusionRemainingSeconds();
	if (RemainingText) RemainingText->SetText(Remaining <= 0 ? NSLOCTEXT("Fusion", "Completed", "완료")
		: FText::FromString(FGameClockRules::FormatCountdown(Remaining)));
	if (ProgressBar) ProgressBar->SetPercent(System->GetFusionProgress());
	if (ClaimButton)
	{
		ClaimButton->SetIsEnabled(bReady);
		ClaimButton->SetButtonText(bReady ? NSLOCTEXT("Fusion", "ClaimCTA", "수령") : NSLOCTEXT("Fusion", "RunningCTA", "진행 중"));
	}
}

int32 UFusionProjectPanelWidget::SlotForBuilding(int32 BuildingIndex) const
{
	const int32* Found = BuildingToSlot.Find(BuildingIndex);
	return Found ? *Found : INDEX_NONE;
}

bool UFusionProjectPanelWidget::IsRunning(FName Key) const
{
	const USideContentSubsystem* System = Side();
	return System && !Key.IsNone() && System->IsFusionActive() && System->GetSideContentData().FusionActiveKey == Key;
}

bool UFusionProjectPanelWidget::SelectCollab(FName Key)
{
	USideContentSubsystem* System = Side();
	UTableManagerSubsystem* TableManager = Tables();
	FFusionProjectTable Row;
	if (!System || !TableManager || !TableManager->GetFusionProject(Key, Row)) return false;
	SelectedKey = Key;
	SlotIndustries = Row.RequiredIndustries;
	// 진행 중인 콜라보는 착수 때 확정된 편성을 그린다 — 자동 추천을 그리면 실제로 일하는 건물과 어긋난다.
	PickedBuildings = IsRunning(Key) ? System->GetSideContentData().FusionBuildingIndices
		: System->AutoPickFusionBuildings(Key);
	// SetNum 만으로 늘리면 부족분이 0 으로 채워진다 — 0 은 유효한 건물 인덱스다.
	while (PickedBuildings.Num() < SlotIndustries.Num()) PickedBuildings.Add(INDEX_NONE);
	PickedBuildings.SetNum(SlotIndustries.Num());
	return true;
}

void UFusionProjectPanelWidget::HandleCard(FName Key)
{
	// 행 데이터가 없으면 선택 상태가 그대로다 — 같은 화면을 다시 그릴 이유도 없다.
	if (SelectCollab(Key)) RefreshBoard();
}

void UFusionProjectPanelWidget::HandleBuilding(int32 BuildingIndex)
{
	// 진행 중에는 편성을 바꿀 수 없다(후보 안내도 그렇게 적혀 있다) — 타일은 열람용으로만 남는다.
	if (IsRunning(SelectedKey)) return;
	const int32 SlotIndex = SlotForBuilding(BuildingIndex);
	if (!PickedBuildings.IsValidIndex(SlotIndex)) return;
	// 같은 타일을 다시 누르면 해제 — 슬롯을 비울 다른 경로가 필요 없다.
	PickedBuildings[SlotIndex] = (PickedBuildings[SlotIndex] == BuildingIndex) ? INDEX_NONE : BuildingIndex;
	RefreshSlots();
	RefreshCandidates();
	RefreshDetail();
}

void UFusionProjectPanelWidget::HandleStart()
{
	if (USideContentSubsystem* System = Side())
	{
		System->StartFusion(SelectedKey, PickedBuildings);   // 실패 사유는 RefreshDetail 이 다시 읽는다
		RefreshBoard();
	}
}

void UFusionProjectPanelWidget::HandleClaim()
{
	USideContentSubsystem* System = Side();
	if (!System || !System->ClaimFusion(ClaimResult)) { RefreshClock(); return; }
	// 결과는 전용 결과 화면이 아니라 공용 보상 토스트로 — 연구소/출석/우편과 같은 경로.
	if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
	{
		FFusionProjectTable Row;
		const FText Title = Tables() && Tables()->GetFusionProject(ClaimResult.FusionKey, Row) ? Row.DisplayName : FText::GetEmpty();
		UIMgr->ShowRewardToast(ClaimResult.Rewards, Title, true);
	}
	// 레벨이 올라 요구치가 바뀌었으므로 편성을 다시 뽑고 전체를 다시 그린다.
	// 수령으로 FusionActiveKey 가 비었으니 여기서는 자동 추천이 나온다.
	// 숨긴 콜라보(세이브에 남아 진행 중이던 키)를 수령해 끝낸 경우 그 카드는 레일에 없다 — 선택을 첫 카드로 되돌린다.
	const TArray<FName> Keys = System->GetAvailableFusionKeys();
	SelectCollab(!Keys.IsEmpty() && !Keys.Contains(SelectedKey) ? Keys[0] : SelectedKey);
	RefreshBoard();
}

FText UFusionProjectPanelWidget::MissingIndustryText(ECompanyType Industry) const
{
	if (UTableManagerSubsystem* TableManager = Tables())
	{
		bool bFound = false;
		const FCompanyInfoTable Info = TableManager->GetCompanyInfo(Industry, bFound);
		if (bFound) return FText::Format(NSLOCTEXT("Fusion", "IndustryMissing", "{0} 건물 필요"), Info.DisplayName);
	}
	return FText::GetEmpty();
}

FText UFusionProjectPanelWidget::BlockReason(EFusionBlockReason Reason) const
{
	switch (Reason)
	{
	case EFusionBlockReason::None: return NSLOCTEXT("Fusion", "ReadyHint", "준비 완료. 참여 건물의 프로젝트는 계속 진행됩니다");
	case EFusionBlockReason::ActiveProject: return NSLOCTEXT("Fusion", "BusyHint", "진행 중인 콜라보를 수령한 뒤 시작할 수 있습니다");
	case EFusionBlockReason::HQLocked: return NSLOCTEXT("Fusion", "HQHint", "본사 레벨을 올리면 시작할 수 있습니다");
	case EFusionBlockReason::MissingIndustry: return Side() ? MissingIndustryText(Side()->GetFusionMissingIndustry(SelectedKey)) : FText::GetEmpty();
	case EFusionBlockReason::NoCrew: return NSLOCTEXT("Fusion", "CrewHint", "참여 건물에 직원을 배치해 주세요");
	case EFusionBlockReason::PowerShort: return NSLOCTEXT("Fusion", "PowerHint", "모든 참여 건물의 팀 파워가 요구치 이상이어야 합니다");
	case EFusionBlockReason::DuplicateBuilding: return NSLOCTEXT("Fusion", "DuplicateHint", "슬롯마다 서로 다른 건물을 골라 주세요");
	case EFusionBlockReason::NotLoaded: return NSLOCTEXT("Fusion", "LoadingHint", "게임 정보를 불러오는 중입니다");
	case EFusionBlockReason::NoProject: return NSLOCTEXT("Fusion", "MissingData", "콜라보 정보를 불러오지 못했습니다");
	default: return NSLOCTEXT("Fusion", "PickHint", "각 슬롯에 참여할 건물을 골라 주세요");
	}
}
