#include "UI/Panel/OfficeEmployeePanelWidget.h"

#include "Components/ComboBoxString.h"
#include "Components/ScrollBox.h"
#include "Components/Spacer.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Core/CGGameInstance.h"
#include "Engine/Engine.h"
#include "Enum/NotificationType.h"
#include "Enum/WidgetType.h"
#include "Manager/EmployeeManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/UIManagerSubsystem.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Employee/EmployeeBadgeCardWidget.h"

using namespace OfficeEmployeeRoster;

namespace OfficeEmployeeRosterPrivate
{
	constexpr int32 BadgeColumns = 2;

	TSet<int32> ReadyIdsOf(const UEmployeeManager& EmpMgr, const TArray<FEmployeeInstance>& Seated, const TArray<FEmployeeInstance>& Bench)
	{
		TSet<int32> Ready;
		for (const TArray<FEmployeeInstance>* Group : { &Seated, &Bench })
		{
			for (const FEmployeeInstance& Employee : *Group)
			{
				if (EmpMgr.CanLevelUpEmployee(Employee.EmployeeID))
				{
					Ready.Add(Employee.EmployeeID);
				}
			}
		}
		return Ready;
	}
}

int32 UOfficeEmployeePanelWidget::FindFirstTodoEmployeeId(const UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	UCGGameInstance* GI = World ? Cast<UCGGameInstance>(World->GetGameInstance()) : nullptr;
	UEmployeeManager* EmpMgr = GI ? GI->GetSubsystem<UEmployeeManager>() : nullptr;
	if (!EmpMgr)
	{
		return INDEX_NONE;
	}
	// 레일 맨 앞 = 사무실 묶음의 할 일 순 첫 직원, 사무실이 비었으면 대기 묶음의 첫 직원
	const int32 BuildingIndex = GI->GetCurrentManagedBuildingIndex();
	TArray<FEmployeeInstance> Roster = EmpMgr->GetEmployeesInBuilding(BuildingIndex);
	if (Roster.Num() == 0)
	{
		Roster = EmpMgr->GetUnassignedEmployeesInBuilding(BuildingIndex);
	}
	SortRosterByTodo(*EmpMgr, Roster);
	return Roster.Num() > 0 ? Roster[0].EmployeeID : INDEX_NONE;
}

// ===== 배선 =====

void UOfficeEmployeePanelWidget::BindRosterControls()
{
	if (FilterCombo) FilterCombo->OnSelectionChanged.AddDynamic(this, &ThisClass::HandleFilterChanged);
	if (SortCycleButton) SortCycleButton->OnClicked().AddUObject(this, &ThisClass::HandleSortCycleClicked);
	if (BulkLevelUpButton) BulkLevelUpButton->OnClicked().AddUObject(this, &ThisClass::HandleBulkLevelUpClicked);
	UpdateSortLabel();
}

void UOfficeEmployeePanelWidget::UnbindRosterControls()
{
	if (FilterCombo) FilterCombo->OnSelectionChanged.RemoveDynamic(this, &ThisClass::HandleFilterChanged);
	if (SortCycleButton) SortCycleButton->OnClicked().RemoveAll(this);
	if (BulkLevelUpButton) BulkLevelUpButton->OnClicked().RemoveAll(this);
}

void UOfficeEmployeePanelWidget::ResetRosterView()
{
	// 다시 열면 전체 · 할 일 순 — 지난번 거르기에 가려 고른 직원이 안 보이는 일을 막는다
	ActiveRosterFilter = FFilterOption();
	RosterSortMode = ESortMode::Todo;
	UpdateSortLabel();
	if (RosterScroll)
	{
		RosterScroll->ScrollToStart();
	}
}

// ===== 데이터 =====

void UOfficeEmployeePanelWidget::GatherRoster(TArray<FEmployeeInstance>& OutSeated, TArray<FEmployeeInstance>& OutBench) const
{
	OutSeated.Reset();
	OutBench.Reset();
	UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance());
	if (!EmployeeManager || !GI)
	{
		return;
	}
	// 직원창 로스터와 같은 범위 — 착석(출장 중 포함) + 같은 건물 소속 대기
	const int32 BuildingIndex = GI->GetCurrentManagedBuildingIndex();
	OutSeated = EmployeeManager->GetEmployeesInBuilding(BuildingIndex);
	OutBench = EmployeeManager->GetUnassignedEmployeesInBuilding(BuildingIndex);
}

TSet<int32> UOfficeEmployeePanelWidget::CollectReadyIds() const
{
	if (!EmployeeManager)
	{
		return TSet<int32>();
	}
	TArray<FEmployeeInstance> Seated, Bench;
	GatherRoster(Seated, Bench);
	return OfficeEmployeeRosterPrivate::ReadyIdsOf(*EmployeeManager, Seated, Bench);
}

void UOfficeEmployeePanelWidget::RebuildFilterOptions(const TArray<FEmployeeInstance>& Seated, const TArray<FEmployeeInstance>& Bench)
{
	UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance());
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;

	TArray<FFilterOption> Options;
	TArray<FString> Labels;
	Options.Add(FFilterOption());
	Labels.Add(NSLOCTEXT("OfficeEmployeePanel", "FilterAll", "전체").ToString());

	// 직능은 이 건물에 있는 것만 — 표시명 SOT = DT_DisciplineDisplay (헤더 직능과 같은 주 직능 판정)
	TSet<int32> PresentSlots;
	for (const TArray<FEmployeeInstance>* Group : { &Seated, &Bench })
	{
		for (const FEmployeeInstance& Employee : *Group)
		{
			const int32 DisciplineSlot = GetPrimaryDisciplineSlot(Employee);
			if (DisciplineSlot != INDEX_NONE)
			{
				PresentSlots.Add(DisciplineSlot);
			}
		}
	}
	TArray<int32> SortedSlots = PresentSlots.Array();
	SortedSlots.Sort();
	for (const int32 DisciplineSlot : SortedSlots)
	{
		const FText Name = TableMgr && GI ? TableMgr->GetDisciplineDisplayName(GI->GetCurrentBuildingCompanyType(), DisciplineSlot) : FText::GetEmpty();
		if (Name.IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("[OfficeEmployeePanel] 직능 %d 표시명 없음 — DT_DisciplineDisplay 확인, 거르기 옵션 생략"), DisciplineSlot);
			continue;
		}
		Options.Add(FFilterOption{ EFilterKind::Discipline, DisciplineSlot });
		Labels.Add(Name.ToString());
	}
	Options.Add(FFilterOption{ EFilterKind::LevelUpReady, INDEX_NONE });
	Labels.Add(NSLOCTEXT("OfficeEmployeePanel", "FilterLevelUp", "레벨업 가능").ToString());
	if (Bench.Num() > 0)
	{
		Options.Add(FFilterOption{ EFilterKind::Bench, INDEX_NONE });
		Labels.Add(NSLOCTEXT("OfficeEmployeePanel", "FilterBench", "대기").ToString());
	}

	if (!Options.Contains(ActiveRosterFilter))
	{
		ActiveRosterFilter = FFilterOption();
	}
	const int32 ActiveIndex = Options.IndexOfByKey(ActiveRosterFilter);
	if (!FilterCombo)
	{
		RosterFilterOptions = MoveTemp(Options);
		return;
	}
	if (Options != RosterFilterOptions || FilterCombo->GetOptionCount() != Options.Num())
	{
		RosterFilterOptions = MoveTemp(Options);
		FilterCombo->ClearOptions();
		for (const FString& Label : Labels)
		{
			FilterCombo->AddOption(Label);
		}
		// ClearOptions 는 표시만 비우고 선택 문자열을 남긴다 — 같은 문자열 재선택은 무시되므로 한 번 다른 칸을 거친다
		FilterCombo->SetSelectedIndex(ActiveIndex == 0 ? 1 : 0);
	}
	if (FilterCombo->GetSelectedIndex() != ActiveIndex)
	{
		FilterCombo->SetSelectedIndex(ActiveIndex);
	}
}

bool UOfficeEmployeePanelWidget::PassesRosterFilter(const FEmployeeInstance& Employee, bool bOnBench) const
{
	switch (ActiveRosterFilter.Kind)
	{
	case EFilterKind::Discipline:   return GetPrimaryDisciplineSlot(Employee) == ActiveRosterFilter.DisciplineSlot;
	// 레벨업 가능 = 모두 레벨업 대상과 같은 범위(일하는 직원)
	case EFilterKind::LevelUpReady: return !bOnBench && RosterReadyIds.Contains(Employee.EmployeeID);
	case EFilterKind::Bench:        return bOnBench;
	default:                        return true;
	}
}

void UOfficeEmployeePanelWidget::SortRosterForDisplay(TArray<FEmployeeInstance>& InOutRoster) const
{
	switch (RosterSortMode)
	{
	case ESortMode::Level:
		InOutRoster.StableSort([](const FEmployeeInstance& A, const FEmployeeInstance& B) { return A.Level > B.Level; });
		break;
	case ESortMode::Name:
		InOutRoster.StableSort([](const FEmployeeInstance& A, const FEmployeeInstance& B) { return A.EmployeeName < B.EmployeeName; });
		break;
	default:
		if (EmployeeManager)
		{
			SortRosterByTodo(*EmployeeManager, InOutRoster);
		}
		break;
	}
}

// ===== 표시 =====

void UOfficeEmployeePanelWidget::RefreshRoster()
{
	TArray<FEmployeeInstance> Seated, Bench;
	GatherRoster(Seated, Bench);
	RosterReadyIds = EmployeeManager ? OfficeEmployeeRosterPrivate::ReadyIdsOf(*EmployeeManager, Seated, Bench) : TSet<int32>();
	const int32 TotalCount = Seated.Num() + Bench.Num();

	RebuildFilterOptions(Seated, Bench);
	RefreshBulkLevelUp(Seated);

	Seated.RemoveAll([this](const FEmployeeInstance& Employee) { return !PassesRosterFilter(Employee, false); });
	Bench.RemoveAll([this](const FEmployeeInstance& Employee) { return !PassesRosterFilter(Employee, true); });
	SortRosterForDisplay(Seated);
	SortRosterForDisplay(Bench);

	RosterOrder.Reset();
	for (const TArray<FEmployeeInstance>* Group : { &Seated, &Bench })
	{
		for (const FEmployeeInstance& Employee : *Group)
		{
			RosterOrder.Add(Employee.EmployeeID);
		}
	}

	FillBadgeGrid(SeatedGrid, SeatedBadges, Seated, false);
	FillBadgeGrid(BenchGrid, BenchBadges, Bench, true);

	const ESlateVisibility BenchVisibility = Bench.Num() > 0 ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed;
	if (BenchHeader) BenchHeader->SetVisibility(BenchVisibility);
	if (BenchGrid) BenchGrid->SetVisibility(BenchVisibility);
	if (BenchCountText) BenchCountText->SetText(FText::AsNumber(Bench.Num()));
	if (RosterCountText)
	{
		RosterCountText->SetText(FText::Format(NSLOCTEXT("OfficeEmployeePanel", "RosterCount", "{0}명"), FText::AsNumber(TotalCount)));
	}
	SyncRosterSelection();
}

void UOfficeEmployeePanelWidget::FillBadgeGrid(UUniformGridPanel* Grid, TArray<TObjectPtr<UEmployeeBadgeCardWidget>>& Pool,
	const TArray<FEmployeeInstance>& Employees, bool bOnBench)
{
	if (!Grid)
	{
		return;
	}
	UClass* BadgeClass = nullptr;
	if (Pool.Num() < Employees.Num())
	{
		UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
		const TSubclassOf<UUserWidget> Found = TableMgr ? TableMgr->GetWidgetClass(EWidgetType::EmployeeBadgeCard) : nullptr;
		BadgeClass = Found && Found->IsChildOf(UEmployeeBadgeCardWidget::StaticClass()) ? Found.Get() : nullptr;
		if (!BadgeClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("[OfficeEmployeePanel] EmployeeBadgeCard 클래스 미등록 — DT_WidgetClass 확인"));
		}
	}

	for (int32 Index = 0; Index < Employees.Num(); ++Index)
	{
		if (!Pool.IsValidIndex(Index))
		{
			UEmployeeBadgeCardWidget* NewBadge = BadgeClass ? CreateWidget<UEmployeeBadgeCardWidget>(this, BadgeClass) : nullptr;
			if (!NewBadge)
			{
				break;
			}
			NewBadge->OnRosterCardClicked.AddDynamic(this, &ThisClass::HandleRosterCardClicked);
			Pool.Add(NewBadge);
		}
		UEmployeeBadgeCardWidget* Badge = Pool[Index];
		const FEmployeeInstance& Employee = Employees[Index];
		Badge->SetBadge(Employee, !bOnBench && RosterReadyIds.Contains(Employee.EmployeeID), bOnBench);
		if (Badge->GetParent() != Grid)
		{
			if (UUniformGridSlot* GridSlot = Grid->AddChildToUniformGrid(Badge, Index / OfficeEmployeeRosterPrivate::BadgeColumns,
				Index % OfficeEmployeeRosterPrivate::BadgeColumns))
			{
				GridSlot->SetHorizontalAlignment(HAlign_Fill);
				GridSlot->SetVerticalAlignment(VAlign_Fill);
			}
		}
	}
	for (int32 Index = Employees.Num(); Index < Pool.Num(); ++Index)
	{
		if (Pool[Index] && Pool[Index]->GetParent())
		{
			Pool[Index]->RemoveFromParent();
		}
	}

	// 한 장뿐이면 UniformGrid 가 1열로 줄어 카드가 레일 폭을 다 차지한다 — 두 번째 칸을 빈 칸으로 잡아 둔다
	USpacer* Filler = nullptr;
	for (UWidget* GridChild : Grid->GetAllChildren())
	{
		Filler = Filler ? Filler : Cast<USpacer>(GridChild);
	}
	const bool bNeedsFiller = Employees.Num() > 0 && Employees.Num() < OfficeEmployeeRosterPrivate::BadgeColumns;
	if (bNeedsFiller && !Filler && WidgetTree)
	{
		Grid->AddChildToUniformGrid(WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass()), 0,
			OfficeEmployeeRosterPrivate::BadgeColumns - 1);
	}
	else if (!bNeedsFiller && Filler)
	{
		Filler->RemoveFromParent();
	}
}

void UOfficeEmployeePanelWidget::RefreshBulkLevelUp(const TArray<FEmployeeInstance>& Seated)
{
	int32 ReadyCount = 0;
	for (const FEmployeeInstance& Employee : Seated)
	{
		ReadyCount += RosterReadyIds.Contains(Employee.EmployeeID) ? 1 : 0;
	}
	if (BulkLevelUpCountText)
	{
		BulkLevelUpCountText->SetText(FText::Format(NSLOCTEXT("OfficeEmployeePanel", "BulkLevelUpCount", "{0}명"), FText::AsNumber(ReadyCount)));
	}
	if (BulkLevelUpBar)
	{
		BulkLevelUpBar->SetVisibility(ReadyCount > 0 ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UOfficeEmployeePanelWidget::SyncRosterSelection(bool bScrollIntoView)
{
	UEmployeeBadgeCardWidget* SelectedBadge = nullptr;
	for (const TArray<TObjectPtr<UEmployeeBadgeCardWidget>>* Pool : { &SeatedBadges, &BenchBadges })
	{
		for (UEmployeeBadgeCardWidget* Badge : *Pool)
		{
			if (!Badge)
			{
				continue;
			}
			const bool bSelected = Badge->GetParent() != nullptr && Badge->GetBadgeEmployeeId() == SelectedEmployeeId;
			Badge->SetBadgeSelected(bSelected);
			if (bSelected)
			{
				SelectedBadge = Badge;
			}
		}
	}
	if (bScrollIntoView && SelectedBadge && RosterScroll)
	{
		RosterScroll->ScrollWidgetIntoView(SelectedBadge, /*AnimateScroll*/ true, EDescendantScrollDestination::IntoView, /*Padding*/ 12.f);
	}
}

void UOfficeEmployeePanelWidget::UpdateSortLabel()
{
	if (!SortCycleButton)
	{
		return;
	}
	switch (RosterSortMode)
	{
	case ESortMode::Level: SortCycleButton->SetButtonText(NSLOCTEXT("OfficeEmployeePanel", "SortLevel", "레벨순")); break;
	case ESortMode::Name:  SortCycleButton->SetButtonText(NSLOCTEXT("OfficeEmployeePanel", "SortName", "이름순")); break;
	default:               SortCycleButton->SetButtonText(NSLOCTEXT("OfficeEmployeePanel", "SortTodo", "추천순")); break;
	}
}

// ===== 조작 =====

void UOfficeEmployeePanelWidget::SetRosterFilterIndex(int32 OptionIndex)
{
	if (!RosterFilterOptions.IsValidIndex(OptionIndex))
	{
		return;
	}
	ActiveRosterFilter = RosterFilterOptions[OptionIndex];
	RefreshRoster();
	if (RosterScroll)
	{
		RosterScroll->ScrollToStart();
	}
}

void UOfficeEmployeePanelWidget::CycleRosterSort()
{
	RosterSortMode = static_cast<ESortMode>((static_cast<uint8>(RosterSortMode) + 1) % static_cast<uint8>(ESortMode::Count));
	UpdateSortLabel();
	RefreshRoster();
	if (RosterScroll)
	{
		RosterScroll->ScrollToStart();
	}
}

int32 UOfficeEmployeePanelWidget::LevelUpAllReady()
{
	if (!EmployeeManager)
	{
		return 0;
	}
	TArray<FEmployeeInstance> Targets, Bench;
	GatherRoster(Targets, Bench);
	Targets.RemoveAll([this](const FEmployeeInstance& Employee) { return !EmployeeManager->CanLevelUpEmployee(Employee.EmployeeID); });
	int32 LeveledCount = 0;
	{
		TGuardValue<bool> BulkGuard(bBulkLevelingUp, true);
		for (const FEmployeeInstance& Employee : Targets)
		{
			if (!EmployeeManager->CanLevelUpEmployee(Employee.EmployeeID) || !EmployeeManager->LevelUpEmployee(Employee.EmployeeID))
			{
				continue;
			}
			++LeveledCount;
		}
	}
	if (LeveledCount == 0)
	{
		return 0;
	}

	RefreshRoster();
	RefreshHeader();
	RefreshGrowth();
	if (UUIManagerSubsystem* UIMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UUIManagerSubsystem>() : nullptr)
	{
		UIMgr->ShowNotification(FText::Format(NSLOCTEXT("OfficeEmployeePanel", "BulkLevelUpDone", "{0}명을 레벨업했습니다"),
			FText::AsNumber(LeveledCount)), 3.0f, ENotificationType::Success);
	}
	return LeveledCount;
}

// ===== 핸들러 =====

void UOfficeEmployeePanelWidget::HandleFilterChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	// 코드가 옵션을 다시 채우며 고르는 것(Direct)은 사용자 선택이 아니다
	if (SelectionType == ESelectInfo::Direct || !FilterCombo)
	{
		return;
	}
	SetRosterFilterIndex(FilterCombo->GetSelectedIndex());
}

void UOfficeEmployeePanelWidget::HandleSortCycleClicked()
{
	CycleRosterSort();
}

void UOfficeEmployeePanelWidget::HandleBulkLevelUpClicked()
{
	LevelUpAllReady();
}

void UOfficeEmployeePanelWidget::HandleEmployeeExperienceChanged(int32 EmployeeID)
{
	if (bBulkLevelingUp)
	{
		return;
	}
	const UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance());
	const FEmployeeInstance* Employee = EmployeeManager ? EmployeeManager->FindEmployee(EmployeeID) : nullptr;
	if (!GI || !Employee || Employee->AssignedBuildingIndex != GI->GetCurrentManagedBuildingIndex()) return;
	// 경험치 신호가 레벨업 가능 집합(정렬·표시·인원 칩)을 바꾼 경우만 다시 채운다.
	if (RosterReadyIds.Contains(EmployeeID) != EmployeeManager->CanLevelUpEmployee(EmployeeID))
	{
		RefreshRoster();
	}
}

void UOfficeEmployeePanelWidget::HandleEmployeePortraitUpdated(int32 EmployeeId, int64 Revision)
{
	// 카드 초상 캐시는 파일 시각으로 새 촬영을 알아챈다 — 다시 채우기만 하면 된다
	RefreshRoster();
}
