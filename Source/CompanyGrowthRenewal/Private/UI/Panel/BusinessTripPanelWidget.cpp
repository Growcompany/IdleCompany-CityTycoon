#include "UI/Panel/BusinessTripPanelWidget.h"
#include "UI/Element/Common/ModalFrameWidget.h"

#include "Blueprint/WidgetTree.h"
#include "CommonTextBlock.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/GameInstance.h"

#include "Data/EmployeeTypes.h"
#include "Data/SideContentRules.h"
#include "Enum/WidgetType.h"
#include "Manager/EmployeeManager.h"
#include "Manager/GameClockSubsystem.h"
#include "Manager/SideContentSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "Table/BusinessTripCardTable.h"
#include "Table/CompanyInfoTable.h"
#include "Table/MissionTable.h"
#include "Table/SideDungeonLootTable.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Common/RewardChipUtils.h"
#include "UI/Element/Employee/EmployeeRosterCardWidget.h"
#include "UI/Element/SideContent/BusinessTripCardWidget.h"
#include "UI/Element/SideContent/BusinessTripCrewCardWidget.h"
#include "UI/Element/SideContent/BusinessTripProgressWidget.h"

namespace
{
	// unity 빌드가 익명 namespace 를 한 블롭으로 합치므로 이름을 모듈 고유로 둔다
	constexpr int32 TripPaneSelect = 0;
	constexpr int32 TripPaneProgress = 1;

	/** 레일 정렬 — 벤치 먼저, 그 안에서 요구 직능 점수 높은 순. [자동 선발]이 채우는 순서와 같다. */
	void SortTripRail(TArray<FEmployeeInstance>& Roster, EProductionDiscipline Discipline)
	{
		Roster.Sort([Discipline](const FEmployeeInstance& A, const FEmployeeInstance& B)
		{
			const bool bBenchA = (A.AssignedBuildingIndex == INDEX_NONE);
			const bool bBenchB = (B.AssignedBuildingIndex == INDEX_NONE);
			if (bBenchA != bBenchB) return bBenchA;

			const int32 ScoreA = FSideContentRules::GetDisciplinePoints(A, Discipline);
			const int32 ScoreB = FSideContentRules::GetDisciplinePoints(B, Discipline);
			if (ScoreA != ScoreB) return ScoreA > ScoreB;

			return A.EmployeeID < B.EmployeeID;
		});
	}
}

void UBusinessTripPanelWidget::Configure(int32 InBuildingIndex, ECompanyType InCompanyType)
{
	BuildingIndex = InBuildingIndex;
	CompanyType = InCompanyType;

	SelectedCardKey = NAME_None;
	PickedCrew.Reset();
	bShowingResult = false;

	RefreshAll();
}

void UBusinessTripPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	for (UBusinessTripCardWidget* Card : GetCardWidgets())
	{
		Card->OnTripCardClicked.AddUObject(this, &UBusinessTripPanelWidget::HandleCardClicked);
	}

	if (AutoPickButton) AutoPickButton->OnClicked().AddUObject(this, &UBusinessTripPanelWidget::HandleAutoPickClicked);
	if (DepartButton)   DepartButton->OnClicked().AddUObject(this, &UBusinessTripPanelWidget::HandleDepartClicked);
	if (ClaimButton)    ClaimButton->OnClicked().AddUObject(this, &UBusinessTripPanelWidget::HandleClaimClicked);

	if (USideContentSubsystem* Side = GetSideContent())
	{
		SideContentChangedHandle = Side->OnSideContentChanged.AddUObject(this, &UBusinessTripPanelWidget::HandleSideContentChanged);
	}

	// 카드 갱신은 자정 하나뿐이라 시계에 직접 묻는다 — 서브시스템에 같은 값을 또 두지 않는다
	if (RefreshCountdownText)
	{
		TWeakObjectPtr<UGameClockSubsystem> Clock =
			GetGameInstance() ? GetGameInstance()->GetSubsystem<UGameClockSubsystem>() : nullptr;

		RefreshTicker.Start(GetWorld(), RefreshCountdownText,
			[Clock]() { return Clock.IsValid() ? Clock->GetSecondsUntilDayRoll() : 0; },
			TEXT("다음 갱신 "));
	}
}

void UBusinessTripPanelWidget::NativeDestruct()
{
	RefreshTicker.Stop();

	for (UBusinessTripCardWidget* Card : GetCardWidgets())
	{
		Card->OnTripCardClicked.RemoveAll(this);
	}
	for (const TObjectPtr<UBusinessTripCrewCardWidget>& Chip : CrewChips)
	{
		if (Chip) Chip->OnRosterCardClicked.RemoveAll(this);
	}

	if (AutoPickButton) AutoPickButton->OnClicked().RemoveAll(this);
	if (DepartButton)   DepartButton->OnClicked().RemoveAll(this);
	if (ClaimButton)    ClaimButton->OnClicked().RemoveAll(this);

	if (USideContentSubsystem* Side = GetSideContent())
	{
		Side->OnSideContentChanged.Remove(SideContentChangedHandle);
	}
	SideContentChangedHandle.Reset();

	Super::NativeDestruct();
}

void UBusinessTripPanelWidget::RefreshAll()
{
	USideContentSubsystem* Side = GetSideContent();
	if (!Side || BuildingIndex == INDEX_NONE) return;

	// 건물 이름은 싣지 않는다 — 오피스는 이미 그 건물 안이고(허브 폐기), 건물별 고유 이름이라는 것이
	// 이 게임엔 없다. 예전엔 "건물 N" 을 지어내 제목과 메타 줄에 두 번 찍었는데 둘 다 의미가 없었다.
	if (ModalFrame)
	{
		ModalFrame->SetTitle(NSLOCTEXT("BusinessTrip", "PanelTitle", "출장"));
	}

	// 산업 이름·색·글리프는 DT_CompanyInfo 가 단일 진실 — 코드 switch 금지
	UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (TableMgr)
	{
		bool bHasInfo = false;
		const FCompanyInfoTable Info = TableMgr->GetCompanyInfo(CompanyType, bHasInfo);
		if (bHasInfo)
		{
			if (IndustryText) IndustryText->SetText(Info.DisplayName);
			if (IndustryPlate) IndustryPlate->SetColorAndOpacity(Info.AccentColor);
			if (HeaderAurora)
			{
				// IT · 반도체 시그니처색은 기능 블루와 이웃이라 진하게 깔면 선택 링이 묻힌다
				const bool bBlueAdjacent = (CompanyType == ECompanyType::IT || CompanyType == ECompanyType::Semiconductor);
				FLinearColor Tint = Info.AccentColor;
				Tint.A = bBlueAdjacent ? 0.30f : 0.45f;
				HeaderAurora->SetColorAndOpacity(Tint);
			}
			if (IndustryGlyph)
			{
				if (UTexture2D* Glyph = Info.GlyphIcon.LoadSynchronous())
				{
					IndustryGlyph->SetBrushFromTexture(Glyph, /*bMatchSize=*/false);
				}
			}
		}
	}

	if (TeamPowerText)
	{
		// 공통 축과 같은 식을 쓴다 — 연수원 화면의 같은 칩과 수치가 어긋나면 안 된다
		UEmployeeManager* EmployeeMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UEmployeeManager>() : nullptr;
		float PowerSum = 0.f;
		if (EmployeeMgr)
		{
			TArray<int32> TeamIDs;
			for (const FEmployeeInstance& Employee : EmployeeMgr->GetActiveEmployeesInBuilding(BuildingIndex))
			{
				TeamIDs.Add(Employee.EmployeeID);
			}

			TArray<float> Power;
			FSideContentRules::ComputeTeamPower(EmployeeMgr->GetAllEmployeesRef(), TeamIDs, Power);
			for (float P : Power) PowerSum += P;
		}
		TeamPowerText->SetText(FText::Format(
			NSLOCTEXT("BusinessTrip", "TeamPower", "팀 파워 {0}"), FText::AsNumber(FMath::RoundToInt(PowerSum))));
	}

	const bool bActive = Side->IsDispatchActive(BuildingIndex);

	if (SlotStateText)
	{
		FText State;
		if (bShowingResult)              State = NSLOCTEXT("BusinessTrip", "SlotDone", "정산 완료");
		else if (Side->IsDispatchClaimable(BuildingIndex)) State = NSLOCTEXT("BusinessTrip", "SlotArrived", "도착");
		else if (bActive)                State = NSLOCTEXT("BusinessTrip", "SlotBusy", "출장 중");
		else                             State = NSLOCTEXT("BusinessTrip", "SlotEmpty", "비어 있음");

		SlotStateText->SetText(FText::Format(NSLOCTEXT("BusinessTrip", "SlotChip", "슬롯 1 · {0}"), State));
	}

	// 결과 카드는 자기 성공이 쏜 방송에 지워지면 안 된다 — 닫기·확인 전까지 버틴다
	if (bShowingResult)
	{
		if (PaneSwitcher) PaneSwitcher->SetActiveWidgetIndex(TripPaneProgress);
		return;
	}

	if (PaneSwitcher) PaneSwitcher->SetActiveWidgetIndex(bActive ? TripPaneProgress : TripPaneSelect);

	if (bActive)
	{
		RefreshProgressPane();
	}
	else
	{
		if (ResultBox) ResultBox->SetVisibility(ESlateVisibility::Collapsed);
		if (ProgressWidget) ProgressWidget->StopCountdown();
		RebuildCards();
		RebuildCrewRail();
		RefreshJudgement();
	}
}

TArray<UBusinessTripCardWidget*> UBusinessTripPanelWidget::GetCardWidgets() const
{
	TArray<UBusinessTripCardWidget*> Out;
	Out.Reserve(3);
	if (Card0) Out.Add(Card0);
	if (Card1) Out.Add(Card1);
	if (Card2) Out.Add(Card2);
	return Out;
}

void UBusinessTripPanelWidget::RebuildCards()
{
	USideContentSubsystem* Side = GetSideContent();
	UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!Side || !TableMgr) return;

	const TArray<FName> CardKeys = Side->GetDispatchCards(BuildingIndex);

	// 골라 둔 카드가 자정 갱신으로 사라졌을 수 있다
	if (!SelectedCardKey.IsNone() && !CardKeys.Contains(SelectedCardKey))
	{
		SelectedCardKey = NAME_None;
		PickedCrew.Reset();
	}

	const TArray<UBusinessTripCardWidget*> Cards = GetCardWidgets();
	for (int32 i = 0; i < Cards.Num(); ++i)
	{
		UBusinessTripCardWidget* Card = Cards[i];

		FBusinessTripCardTable Row;
		// DT 행이 없는 카드는 숨긴다 — 빈 카드를 그리면 왜 못 누르는지 알 수 없다(loud failure 는 로그가 맡는다)
		if (!CardKeys.IsValidIndex(i) || !TableMgr->GetBusinessTripCard(CardKeys[i], Row))
		{
			if (CardKeys.IsValidIndex(i))
			{
				UE_LOG(LogTemp, Warning, TEXT("[BusinessTripPanel] 카드 행 없음: %s (DT_BusinessTripCard 확인)"), *CardKeys[i].ToString());
			}
			Card->SetVisibility(ESlateVisibility::Collapsed);
			continue;
		}

		Card->SetVisibility(ESlateVisibility::Visible);
		Card->SetCard(CardKeys[i], Row, GetDisciplineName(Row.RequiredDiscipline));
		Card->SetSelected(CardKeys[i] == SelectedCardKey);
		Card->SetLootPreview(TableMgr->GetSideDungeonLootRows(Row.LootTableKey), Row.LootStage);
	}
}

void UBusinessTripPanelWidget::RebuildCrewRail()
{
	if (!CrewRail) return;

	for (const TObjectPtr<UBusinessTripCrewCardWidget>& OldChip : CrewChips)
	{
		if (OldChip) OldChip->OnRosterCardClicked.RemoveAll(this);
	}
	CrewChips.Reset();
	CrewRail->ClearChildren();

	UEmployeeManager* EmployeeMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UEmployeeManager>() : nullptr;
	UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!EmployeeMgr || !TableMgr) return;

	const TSubclassOf<UUserWidget> ChipClass = TableMgr->GetWidgetClass(EWidgetType::BusinessTripCrewChip);
	if (!ChipClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BusinessTripPanel] BusinessTripCrewChip 위젯 클래스 미등록 (DT_WidgetClass 확인)"));
		return;
	}

	FBusinessTripCardTable Row;
	const bool bHasCard = !SelectedCardKey.IsNone() && TableMgr->GetBusinessTripCard(SelectedCardKey, Row);

	// 벤치 + 이 건물 배치 팀만 데려갈 수 있다. 남의 건물 직원과 이미 출장 간 직원은 레일에 서지 않는다.
	const UGameClockSubsystem* Clock = GetGameInstance()->GetSubsystem<UGameClockSubsystem>();
	const int64 Now = Clock ? Clock->GetServerNowUnix() : 0;

	TArray<FEmployeeInstance> Rail;
	int32 TeamCount = 0;
	int32 BenchCount = 0;
	for (const FEmployeeInstance& Employee : EmployeeMgr->GetAllEmployeesRef())
	{
		const bool bBench = (Employee.AssignedBuildingIndex == INDEX_NONE);
		if (!bBench && Employee.AssignedBuildingIndex != BuildingIndex) continue;
		if (FSideContentRules::IsBusy(Employee, Now)) continue;

		Rail.Add(Employee);
		if (bBench) ++BenchCount; else ++TeamCount;
	}

	SortTripRail(Rail, bHasCard ? Row.RequiredDiscipline : EProductionDiscipline::Plan);

	for (const FEmployeeInstance& Employee : Rail)
	{
		UBusinessTripCrewCardWidget* Chip = CreateWidget<UBusinessTripCrewCardWidget>(this, ChipClass);
		if (!Chip) continue;

		// 초상·이름·Lv·별·등급 젬은 로스터 카드가 스스로 채운다(초상 크기는 WBP 의 PortraitDisplaySize)
		Chip->SetEmployeeInfo(Employee);

		// 카드를 고르기 전에는 어느 직능을 보여줄지 정해지지 않았다 — 점수 자리를 비워 둔다
		const int32 Score = bHasCard ? FSideContentRules::GetDisciplinePoints(Employee, Row.RequiredDiscipline) : INDEX_NONE;
		Chip->SetContextScore(bHasCard ? GetDisciplineName(Row.RequiredDiscipline) : FText::GetEmpty(), Score);
		Chip->SetBuildingTeam(Employee.AssignedBuildingIndex == BuildingIndex);
		Chip->SetSelectedVisual(PickedCrew.Contains(Employee.EmployeeID));
		Chip->OnRosterCardClicked.AddDynamic(this, &UBusinessTripPanelWidget::HandleCrewToggled);

		if (UHorizontalBoxSlot* BoxSlot = CrewRail->AddChildToHorizontalBox(Chip))
		{
			BoxSlot->SetPadding(FMargin(0.f, 0.f, 14.f, 0.f));
		}
		CrewChips.Add(Chip);
	}

	if (RosterNoteText)
	{
		RosterNoteText->SetText(FText::Format(
			NSLOCTEXT("BusinessTrip", "RosterNote", "배치 팀 {0}명 · 벤치 {1}명"),
			FText::AsNumber(TeamCount), FText::AsNumber(BenchCount)));
	}
}

void UBusinessTripPanelWidget::RebuildPickSlots(int32 InNeed, int32 InPicked)
{
	if (!PickSlotBox || !WidgetTree) return;

	// 샘플 자체가 박스 안에 있으면 ClearChildren 이 떼어 가므로 브러시를 먼저 복사해 둔다
	FSlateBrush DotBrush;
	if (PickSlotSample) DotBrush = PickSlotSample->GetBrush();

	PickSlotBox->ClearChildren();

	for (int32 i = 0; i < InNeed; ++i)
	{
		UImage* Dot = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Dot->SetBrush(DotBrush);
		Dot->SetColorAndOpacity(i < InPicked ? PickSlotFilledColor : PickSlotEmptyColor);

		if (UHorizontalBoxSlot* BoxSlot = PickSlotBox->AddChildToHorizontalBox(Dot))
		{
			BoxSlot->SetVerticalAlignment(VAlign_Center);
			BoxSlot->SetPadding(FMargin(i == 0 ? 0.f : 10.f, 0.f, 0.f, 0.f));
		}
	}
}

void UBusinessTripPanelWidget::RefreshJudgement()
{
	USideContentSubsystem* Side = GetSideContent();
	UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!Side || !TableMgr) return;

	FBusinessTripCardTable Row;
	const bool bHasCard = !SelectedCardKey.IsNone() && TableMgr->GetBusinessTripCard(SelectedCardKey, Row);

	if (!bHasCard)
	{
		if (RequireText) RequireText->SetText(NSLOCTEXT("BusinessTrip", "ReqNone", "요구 ―"));
		if (RequireBar)
		{
			RequireBar->SetPercent(0.f);
			RequireBar->SetFillColorAndOpacity(BarOkColor);
		}
		if (HaveText)
		{
			HaveText->SetText(NSLOCTEXT("BusinessTrip", "HaveNone", "카드를 고르면 판정이 보입니다"));
			HaveText->SetColorAndOpacity(FSlateColor(HaveOkColor));
		}
		if (HintText) HintText->SetText(NSLOCTEXT("BusinessTrip", "HintNoCard", "카드를 고르면 이 건물 직원 중에서 보낼 사람을 고를 수 있습니다."));
		if (AutoPickButton) AutoPickButton->SetIsEnabled(false);
		if (DepartButton)
		{
			DepartButton->SetIsEnabled(false);
			DepartButton->SetButtonText(NSLOCTEXT("BusinessTrip", "DepartNoCard", "출장지 선택"));
		}

		RebuildPickSlots(0, 0);
		if (PickCountText) PickCountText->SetText(NSLOCTEXT("BusinessTrip", "PickNone", "―"));
		for (UBusinessTripCardWidget* Card : GetCardWidgets())
		{
			Card->SetCardState(EBusinessTripCardState::Normal);
		}
		return;
	}

	const FText DisciplineName = GetDisciplineName(Row.RequiredDiscipline);
	const int32 Picked = PickedCrew.Num();
	const int32 Score = Side->GetDispatchCrewScore(SelectedCardKey, PickedCrew);
	const bool bCrewOk = Picked >= Row.CrewCount;
	const bool bScoreOk = Score >= Row.RequiredScoreSum;

	RebuildPickSlots(Row.CrewCount, Picked);
	if (PickCountText)
	{
		PickCountText->SetText(FText::Format(NSLOCTEXT("BusinessTrip", "PickCount", "{0} / {1}"),
			FText::AsNumber(Picked), FText::AsNumber(Row.CrewCount)));
	}

	if (RequireText)
	{
		RequireText->SetText(FText::Format(NSLOCTEXT("BusinessTrip", "Require", "요구 {0} 합 {1}"),
			DisciplineName, FText::AsNumber(Row.RequiredScoreSum)));
	}
	if (RequireBar)
	{
		RequireBar->SetPercent(Row.RequiredScoreSum > 0
			? FMath::Clamp(static_cast<float>(Score) / static_cast<float>(Row.RequiredScoreSum), 0.f, 1.f)
			: 1.f);
		RequireBar->SetFillColorAndOpacity(bScoreOk ? BarOkColor : BarShortColor);
	}
	if (HaveText)
	{
		HaveText->SetText(FText::Format(NSLOCTEXT("BusinessTrip", "Have", "선발 {0} 합 {1}"),
			DisciplineName, FText::AsNumber(Score)));
		// 다크 위 빨강은 안 읽혀서 미달은 앰버로 뺀다
		HaveText->SetColorAndOpacity(bScoreOk ? FSlateColor(HaveOkColor) : FSlateColor(ShortValueColor));
	}

	// 선택 카드의 상태 띠 — 지금 이 편성으로 왜 못 떠나는지만 말한다
	for (UBusinessTripCardWidget* Card : GetCardWidgets())
	{
		if (Card->GetCardKey() != SelectedCardKey)
		{
			Card->SetCardState(EBusinessTripCardState::Normal);
			continue;
		}
		Card->SetCardState(!bCrewOk ? EBusinessTripCardState::CrewShort
			: (!bScoreOk ? EBusinessTripCardState::DisciplineShort : EBusinessTripCardState::Normal));
	}

	// 레일 칩 수로 인원을 세면 안 된다 — DT_WidgetClass 행이 빠져도 레일이 비고, 그러면
	// 설정 실수가 "이 건물에 직원이 없습니다" 라는 거짓말로 나간다
	int32 TeamTaken = 0;
	int32 AvailableCrew = 0;
	if (UEmployeeManager* EmployeeMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UEmployeeManager>() : nullptr)
	{
		const UGameClockSubsystem* Clock = GetGameInstance()->GetSubsystem<UGameClockSubsystem>();
		AvailableCrew = FSideContentRules::CountAvailableCrew(
			EmployeeMgr->GetAllEmployeesRef(), BuildingIndex, Clock ? Clock->GetServerNowUnix() : 0);

		for (const FEmployeeInstance& Employee : EmployeeMgr->GetAllEmployeesRef())
		{
			if (PickedCrew.Contains(Employee.EmployeeID) && Employee.AssignedBuildingIndex == BuildingIndex)
			{
				++TeamTaken;
			}
		}
	}

	if (HintText)
	{
		FText Hint;
		if (AvailableCrew == 0)
		{
			Hint = NSLOCTEXT("BusinessTrip", "HintNoRoster", "이 건물에 직원이 없습니다. 직원을 배치하면 출장을 보낼 수 있습니다.");
		}
		else if (!bCrewOk)
		{
			Hint = FText::Format(NSLOCTEXT("BusinessTrip", "HintCrewShort",
				"보낼 직원이 {0}명 더 필요합니다. 직원을 선택하거나 [자동 선발]을 누르세요."),
				FText::AsNumber(Row.CrewCount - Picked));
		}
		else if (!bScoreOk)
		{
			Hint = FText::Format(NSLOCTEXT("BusinessTrip", "HintScoreShort",
				"{0} 점수가 {1} 모자랍니다. 직능이 높은 직원으로 바꾸면 채울 수 있습니다."),
				DisciplineName, FText::AsNumber(Row.RequiredScoreSum - Score));
		}
		else if (TeamTaken > 0)
		{
			Hint = FText::Format(NSLOCTEXT("BusinessTrip", "HintTeamTaken",
				"배치 팀에서 {0}명을 데려갑니다. 돌아올 때까지 이 건물 프로젝트 기여가 줄어듭니다."),
				FText::AsNumber(TeamTaken));
		}
		else
		{
			Hint = FText::Format(NSLOCTEXT("BusinessTrip", "HintReady", "벤치 {0}명을 {1} 동안 보냅니다. 실패는 없습니다."),
				FText::AsNumber(Picked), UBusinessTripCardWidget::FormatTripDuration(Row.DurationSeconds));
		}
		HintText->SetText(Hint);
	}

	if (AutoPickButton) AutoPickButton->SetIsEnabled(AvailableCrew > 0);
	if (DepartButton)
	{
		// 서버 시각이 무효면 출발도 막는다 — 도착 판정이 로컬 시계로 넘어가지 않게 하는 유일한 지점
		const bool bServerTimeOk = IsServerTimeValidNow();
		const EDispatchBlockReason Reason = Side->GetDispatchBlockReason(BuildingIndex, SelectedCardKey, PickedCrew);
		const bool bCanDepart = bServerTimeOk && Reason == EDispatchBlockReason::None;
		DepartButton->SetIsEnabled(bCanDepart);
		DepartButton->SetButtonText(bCanDepart
			? DepartLabel
			: MakeDepartBlockedLabel(bServerTimeOk, Reason, DisciplineName, Row.CrewCount - Picked, Row.RequiredScoreSum - Score));
	}
}

FText UBusinessTripPanelWidget::MakeDepartBlockedLabel(bool bInServerTimeOk, EDispatchBlockReason InReason,
	const FText& InDisciplineName, int32 InCrewShort, int32 InScoreShort) const
{
	// 회색 판만 두고 사유를 딴 데 적으면 버튼을 눌러 보고 나서야 이유를 찾게 된다(레퍼런스 §2.1-7)
	if (!bInServerTimeOk) return NSLOCTEXT("BusinessTrip", "DepartNoClock", "서버 시각 확인 중");

	switch (InReason)
	{
	case EDispatchBlockReason::NoCard:
		return NSLOCTEXT("BusinessTrip", "DepartNoCard", "출장지 선택");
	case EDispatchBlockReason::SlotBusy:
		return NSLOCTEXT("BusinessTrip", "DepartBusy", "출장 진행 중");
	case EDispatchBlockReason::CompanyLocked:
		return NSLOCTEXT("BusinessTrip", "DepartLocked", "회사 레벨 부족");
	case EDispatchBlockReason::CrewCountMismatch:
		return InCrewShort > 0
			? FText::Format(NSLOCTEXT("BusinessTrip", "DepartCrewShort", "직원 {0}명 부족"), FText::AsNumber(InCrewShort))
			: NSLOCTEXT("BusinessTrip", "DepartCrewPick", "직원 선발 필요");
	case EDispatchBlockReason::CrewUnavailable:
		return NSLOCTEXT("BusinessTrip", "DepartCrewGone", "직원 선발 필요");
	case EDispatchBlockReason::DisciplineShort:
		return FText::Format(NSLOCTEXT("BusinessTrip", "DepartScoreShort", "{0} {1} 부족"),
			InDisciplineName, FText::AsNumber(FMath::Max(InScoreShort, 0)));
	default:
		return DepartLabel;
	}
}

void UBusinessTripPanelWidget::RefreshProgressPane()
{
	USideContentSubsystem* Side = GetSideContent();
	UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	UEmployeeManager* EmployeeMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UEmployeeManager>() : nullptr;
	if (!Side || !TableMgr || !EmployeeMgr || !ProgressWidget) return;

	const FSideBuildingState* State = Side->FindBuilding(BuildingIndex);
	if (!State) return;

	FBusinessTripCardTable Row;
	if (!TableMgr->GetBusinessTripCard(State->DispatchSlot.CardKey, Row)) return;

	TArray<FBusinessTripCrewView> Crew;
	for (int32 EmployeeID : State->DispatchSlot.EmployeeIDs)
	{
		const FEmployeeInstance* Employee = EmployeeMgr->GetAllEmployeesRef().FindByPredicate(
			[EmployeeID](const FEmployeeInstance& E) { return E.EmployeeID == EmployeeID; });
		if (!Employee) continue;

		FBusinessTripCrewView View;
		View.Name = Employee->EmployeeName;
		View.bBuildingTeam = (Employee->AssignedBuildingIndex == BuildingIndex);
		View.Portrait = UEmployeeRosterCardWidget::LoadPortraitTexture(EmployeeID, PortraitSize);
		Crew.Add(View);
	}

	TWeakObjectPtr<USideContentSubsystem> WeakSide = Side;
	const int32 Building = BuildingIndex;
	ProgressWidget->SetTrip(Row, GetDisciplineName(Row.RequiredDiscipline), Crew,
		[WeakSide, Building]() -> int64
		{
			return WeakSide.IsValid() ? static_cast<int64>(WeakSide->GetDispatchRemainingSeconds(Building)) : 0;
		});

	ProgressWidget->SetLootPreview(TableMgr->GetSideDungeonLootRows(Row.LootTableKey), Row.LootStage);

	const int64 Span = FMath::Max<int64>(1, State->DispatchSlot.EndUnix - State->DispatchSlot.StartUnix);
	const int64 Left = FMath::Max<int64>(0, static_cast<int64>(Side->GetDispatchRemainingSeconds(BuildingIndex)));
	ProgressWidget->SetProgress(1.f - static_cast<float>(Left) / static_cast<float>(Span));

	const bool bClaimable = Side->IsDispatchClaimable(BuildingIndex);
	if (ResultBox) ResultBox->SetVisibility(ESlateVisibility::Collapsed);
	if (ClaimButton)
	{
		ClaimButton->SetIsEnabled(bClaimable);
		ClaimButton->SetButtonText(bClaimable
			? NSLOCTEXT("BusinessTrip", "Claim", "수령")
			: NSLOCTEXT("BusinessTrip", "Returning", "돌아오는 중"));
	}
	if (HintTextB)
	{
		HintTextB->SetText(bClaimable
			? NSLOCTEXT("BusinessTrip", "HintClaim", "도착했습니다. [수령]을 누르면 경험치와 드랍을 받습니다.")
			: NSLOCTEXT("BusinessTrip", "HintRunning", "보낸 직원은 돌아올 때까지 책상에 기여하지 않습니다."));
	}
}

void UBusinessTripPanelWidget::ShowResult(const TArray<FMissionReward>& InRewards)
{
	bShowingResult = true;

	if (PaneSwitcher) PaneSwitcher->SetActiveWidgetIndex(TripPaneProgress);
	if (ResultBox) ResultBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	if (ResultRewardBox && WidgetTree)
	{
		// 실제로 받은 것은 확률이 없으므로 게임 전체가 쓰는 보상 칩 문법 그대로 그린다
		CGRewardChip::RenderIcons(*WidgetTree, GetGameInstance(), *ResultRewardBox, InRewards);
	}

	if (ResultLineText)
	{
		ResultLineText->SetText(InRewards.Num() > 0
			? NSLOCTEXT("BusinessTrip", "ResultGot", "출장을 마치고 돌아왔습니다.")
			: NSLOCTEXT("BusinessTrip", "ResultNone", "출장을 마쳤지만 가져온 물건은 없습니다."));
	}
	if (ResultSubText)
	{
		ResultSubText->SetText(NSLOCTEXT("BusinessTrip", "ResultSub", "보낸 직원 모두가 경험치를 받았습니다."));
	}

	if (ClaimButton)
	{
		ClaimButton->SetIsEnabled(true);
		ClaimButton->SetButtonText(NSLOCTEXT("BusinessTrip", "ResultOk", "확인"));
	}
	if (HintTextB)
	{
		HintTextB->SetText(NSLOCTEXT("BusinessTrip", "HintResult", "다음 출장은 바로 보낼 수 있습니다."));
	}
}

USideContentSubsystem* UBusinessTripPanelWidget::GetSideContent() const
{
	UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<USideContentSubsystem>() : nullptr;
}

FText UBusinessTripPanelWidget::GetDisciplineName(EProductionDiscipline InDiscipline) const
{
	UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	return TableMgr ? TableMgr->GetDisciplineDisplayName(CompanyType, static_cast<int32>(InDiscipline)) : FText::GetEmpty();
}

void UBusinessTripPanelWidget::HandleCardClicked(FName InCardKey)
{
	if (SelectedCardKey == InCardKey) return;

	SelectedCardKey = InCardKey;
	// 카드가 바뀌면 요구 직능도 바뀐다 — 앞 카드 기준으로 고른 편성을 물려주지 않는다
	PickedCrew.Reset();

	for (UBusinessTripCardWidget* Card : GetCardWidgets())
	{
		Card->SetSelected(Card->GetCardKey() == SelectedCardKey);
	}

	RebuildCrewRail();
	RefreshJudgement();
}

void UBusinessTripPanelWidget::HandleCrewToggled(int32 InEmployeeID)
{
	USideContentSubsystem* Side = GetSideContent();
	UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!Side || !TableMgr) return;

	FBusinessTripCardTable Row;
	if (SelectedCardKey.IsNone() || !TableMgr->GetBusinessTripCard(SelectedCardKey, Row)) return;

	if (PickedCrew.Contains(InEmployeeID))
	{
		PickedCrew.Remove(InEmployeeID);
	}
	else
	{
		// 정원을 넘기면 가장 먼저 고른 사람이 빠진다 — 해제부터 하라고 막으면 손이 두 번 간다
		if (PickedCrew.Num() >= Row.CrewCount && PickedCrew.Num() > 0)
		{
			PickedCrew.RemoveAt(0);
		}
		PickedCrew.Add(InEmployeeID);
	}

	for (const TObjectPtr<UBusinessTripCrewCardWidget>& Chip : CrewChips)
	{
		if (Chip) Chip->SetSelectedVisual(PickedCrew.Contains(Chip->GetEmployeeID()));
	}
	RefreshJudgement();
}

void UBusinessTripPanelWidget::HandleAutoPickClicked()
{
	USideContentSubsystem* Side = GetSideContent();
	if (!Side || SelectedCardKey.IsNone()) return;

	PickedCrew = Side->AutoPickDispatchCrew(BuildingIndex, SelectedCardKey);

	for (const TObjectPtr<UBusinessTripCrewCardWidget>& Chip : CrewChips)
	{
		if (Chip) Chip->SetSelectedVisual(PickedCrew.Contains(Chip->GetEmployeeID()));
	}
	RefreshJudgement();
}

void UBusinessTripPanelWidget::HandleDepartClicked()
{
	USideContentSubsystem* Side = GetSideContent();
	if (!Side || SelectedCardKey.IsNone()) return;

	// 막힌 이유는 이미 버튼이 비활성으로 말하고 있다 — 여기서는 조용히 되돌아간다
	if (Side->Depart(BuildingIndex, SelectedCardKey, PickedCrew))
	{
		SelectedCardKey = NAME_None;
		PickedCrew.Reset();
	}
	RefreshAll();
}

void UBusinessTripPanelWidget::HandleClaimClicked()
{
	// 결과 화면에서 누른 [확인] — 수령이 아니라 닫기다
	if (bShowingResult)
	{
		bShowingResult = false;
		RefreshAll();
		return;
	}

	USideContentSubsystem* Side = GetSideContent();
	if (!Side) return;

	TArray<FMissionReward> Rewards;
	if (Side->ClaimDispatch(BuildingIndex, Rewards))
	{
		ShowResult(Rewards);
	}
}

void UBusinessTripPanelWidget::HandleSideContentChanged()
{
	RefreshAll();
}
