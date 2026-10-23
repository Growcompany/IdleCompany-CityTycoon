#include "Data/OfficeRosterSummary.h"
#include "Data/EmployeeTypes.h"
#include "Enum/ProductionDiscipline.h"

// 유니티 빌드에서 이름이 겹치지 않게 접두를 붙인다(익명 ns 금지 규약)
static void OfficeRosterAccumulate(const FEmployeeInstance& Emp, TArray<FOfficeRosterLine>& Out)
{
	const int32 Primary = GetPrimaryDisciplineSlot(Emp);
	if (Out.IsValidIndex(Primary))
	{
		++Out[Primary].Headcount;
	}

	const int32 PointCount = FMath::Min(Emp.DisciplinePoints.Num(), Out.Num());
	for (int32 Index = 0; Index < PointCount; ++Index)
	{
		Out[Index].Power += Emp.DisciplinePoints[Index];
	}
}

void OfficeRosterSummary::Build(const TArray<FEmployeeInstance>& Members, TArray<FOfficeRosterLine>& Out, ECompanyType CompanyType)
{
	const int32 LineCount = GetDisciplineCountForCompany(CompanyType);
	Out.Reset(LineCount);
	Out.SetNum(LineCount);
	for (const FEmployeeInstance& Emp : Members)
	{
		OfficeRosterAccumulate(Emp, Out);
	}
}

void OfficeRosterSummary::BuildForBuilding(const TArray<FEmployeeInstance>& AllEmployees, int32 BuildingIndex,
	TArray<FOfficeRosterLine>& Out, int32& OutMemberCount, ECompanyType CompanyType)
{
	Build({}, Out, CompanyType);
	OutMemberCount = 0;
	for (const FEmployeeInstance& Emp : AllEmployees)
	{
		// 인원 칩(GetEmployeeCountInBuilding)과 같은 판정 ― bIsAssigned(책상 배치)는 보지 않는다
		if (BuildingIndex == INDEX_NONE || Emp.AssignedBuildingIndex != BuildingIndex) { continue; }
		++OutMemberCount;
		OfficeRosterAccumulate(Emp, Out);
	}
}
