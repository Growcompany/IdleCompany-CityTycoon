#include "UI/Element/Office/OfficeRosterSummaryWidget.h"
#include "UI/Element/Office/OfficeRosterRowWidget.h"
#include "Components/Image.h"
#include "Core/CGGameInstance.h"
#include "Data/EmployeeTypes.h"
#include "Data/OfficeRosterSummary.h"
#include "Global/GlobalUtilFunctions.h"
#include "Manager/EmployeeManager.h"

void UOfficeRosterSummaryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	PlateSizer.Bind(PlateBG, PlateLine);
	if (UEmployeeManager* EmpMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UEmployeeManager>() : nullptr)
	{
		EmpMgr->OnEmployeeRosterChanged.AddUObject(this, &UOfficeRosterSummaryWidget::HandleRosterChanged);
		// 레벨업 직능 포인트는 DisciplineChanged, 강화·큐브는 StatsChanged 로 온다
		EmpMgr->OnEmployeeDisciplineChanged.AddUObject(this, &UOfficeRosterSummaryWidget::HandleEmployeeChanged);
		EmpMgr->OnEmployeeStatsChanged.AddUObject(this, &UOfficeRosterSummaryWidget::HandleEmployeeChanged);
	}
	Refresh();
}

void UOfficeRosterSummaryWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	PlateSizer.Update();
}

void UOfficeRosterSummaryWidget::NativeDestruct()
{
	if (UEmployeeManager* EmpMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UEmployeeManager>() : nullptr)
	{
		EmpMgr->OnEmployeeRosterChanged.RemoveAll(this);
		EmpMgr->OnEmployeeDisciplineChanged.RemoveAll(this);
		EmpMgr->OnEmployeeStatsChanged.RemoveAll(this);
	}
	Super::NativeDestruct();
}

void UOfficeRosterSummaryWidget::Refresh()
{
	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	UEmployeeManager* EmpMgr = GI ? GI->GetSubsystem<UEmployeeManager>() : nullptr;
	if (!EmpMgr)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	// GetEmployeesInBuilding 은 책상에 앉은 직원(bIsAssigned)만 준다 ― 인원 칩과 같은 집합(대기석 포함)으로 센다
	TArray<FOfficeRosterLine> Lines;
	int32 MemberCount = 0;
	OfficeRosterSummary::BuildForBuilding(EmpMgr->GetAllEmployeesRef(), GI->GetCurrentManagedBuildingIndex(),
		Lines, MemberCount, GI->GetCurrentBuildingCompanyType());
	ApplyLines(Lines, MemberCount);
}

void UOfficeRosterSummaryWidget::ApplyLines(const TArray<FOfficeRosterLine>& Lines, int32 MemberCount)
{
	// 0명이면 6줄이 전부 붉은 0 이라 오류 화면처럼 읽힌다 ― 첫 채용 전엔 판을 접는다
	if (MemberCount <= 0)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	for (int32 Index = 0; Index < Lines.Num(); ++Index)
	{
		if (UOfficeRosterRowWidget* Row = GetRow(Index))
		{
			Row->SetVisibility(ESlateVisibility::HitTestInvisible);
			Row->SetLine(UGlobalUtilFunctions::GetDisciplineIcon(GetDisciplineIconSlot(Index, Lines.Num())), Lines[Index].Headcount,
				UGlobalUtilFunctions::AbbreviateNumber(Lines[Index].Power));
		}
	}
	for (int32 Index = Lines.Num(); Index < static_cast<int32>(EProductionDiscipline::Count); ++Index)
	{
		if (UOfficeRosterRowWidget* Row = GetRow(Index)) { Row->SetVisibility(ESlateVisibility::Collapsed); }
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

UOfficeRosterRowWidget* UOfficeRosterSummaryWidget::GetRow(int32 Index) const
{
	UOfficeRosterRowWidget* const Rows[] = { Row0.Get(), Row1.Get(), Row2.Get(), Row3.Get(), Row4.Get(), Row5.Get() };
	return (Index >= 0 && Index < static_cast<int32>(UE_ARRAY_COUNT(Rows))) ? Rows[Index] : nullptr;
}
