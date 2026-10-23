#include "Misc/AutomationTest.h"
#include "Data/OfficeRosterSummary.h"
#include "Data/EmployeeTypes.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfficeRosterSummaryTest,
	"CGR.Office.RosterSummary.Aggregation",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

// 익명 ns 금지 ― 유니티 TU 병합 시 재정의 충돌
namespace OfficeRosterSummaryTest
{
	// 슬롯 순서 = Plan / Dev / Graphics / Sound / Server / QA
	static FEmployeeInstance MakeEmp(std::initializer_list<int32> Points)
	{
		FEmployeeInstance Emp;
		Emp.DisciplinePoints = TArray<int32>(Points);
		return Emp;
	}
}

bool FOfficeRosterSummaryTest::RunTest(const FString& Parameters)
{
	using OfficeRosterSummaryTest::MakeEmp;
	TArray<FOfficeRosterLine> Lines;

	OfficeRosterSummary::Build({}, Lines);
	TestEqual(TEXT("empty roster still yields 6 lines"), Lines.Num(), 6);
	for (const FOfficeRosterLine& Line : Lines)
	{
		TestEqual(TEXT("empty headcount"), Line.Headcount, 0);
		TestEqual(TEXT("empty power"), Line.Power, (int64)0);
	}

	// 개발 주력 2명 + 그래픽 주력 1명. 개발자도 기획·그래픽·서버 점수를 조금 갖고 있다
	const TArray<FEmployeeInstance> Members = {
		MakeEmp({1, 9, 2, 0, 0, 0}),
		MakeEmp({0, 7, 3, 0, 1, 0}),
		MakeEmp({0, 1, 8, 0, 0, 0}),
	};
	OfficeRosterSummary::Build(Members, Lines);
	TestEqual(TEXT("dev headcount"), Lines[1].Headcount, 2);
	TestEqual(TEXT("graphics headcount"), Lines[2].Headcount, 1);
	TestEqual(TEXT("plan headcount"), Lines[0].Headcount, 0);
	TestEqual(TEXT("dev power sums every member"), Lines[1].Power, (int64)17);
	TestEqual(TEXT("graphics power sums every member"), Lines[2].Power, (int64)13);
	TestEqual(TEXT("plan power exists with zero plan staff"), Lines[0].Power, (int64)1);
	TestEqual(TEXT("server power exists with zero server staff"), Lines[4].Power, (int64)1);

	// 동점이면 앞 슬롯 ― 직원 카드 직능 칩과 같은 판정(GetPrimaryDisciplineSlot)
	OfficeRosterSummary::Build({ MakeEmp({5, 5, 0, 0, 0, 0}) }, Lines);
	TestEqual(TEXT("tie goes to the earlier slot"), Lines[0].Headcount, 1);
	TestEqual(TEXT("tie is not double counted"), Lines[1].Headcount, 0);

	// 직능 포인트가 없는 직원(빈 배열 = 0×6)은 어느 줄에도 세지 않는다
	OfficeRosterSummary::Build({ MakeEmp({}), MakeEmp({0, 0, 0, 0, 0, 0}) }, Lines);
	int32 Counted = 0;
	for (const FOfficeRosterLine& Line : Lines) { Counted += Line.Headcount; }
	TestEqual(TEXT("members without a discipline profile are not counted"), Counted, 0);

	// 합이 int32 를 넘어도 깨지지 않는다
	TArray<FEmployeeInstance> Big;
	for (int32 i = 0; i < 3; ++i) { Big.Add(MakeEmp({0, MAX_int32, 0, 0, 0, 0})); }
	OfficeRosterSummary::Build(Big, Lines);
	TestEqual(TEXT("power is summed in int64"), Lines[1].Power, (int64)MAX_int32 * 3);

	// 건물 소속 = 인원 칩과 같은 집합: 대기석(bIsAssigned=false)도 세고, 다른 건물·무소속은 뺀다
	{
		TArray<FEmployeeInstance> All = { MakeEmp({0, 9, 0, 0, 0, 0}), MakeEmp({7, 0, 0, 0, 0, 0}), MakeEmp({0, 0, 5, 0, 0, 0}), MakeEmp({0, 0, 0, 4, 0, 0}) };
		All[0].AssignedBuildingIndex = 3; All[0].bIsAssigned = true;     // 책상에 앉은 소속 직원
		All[1].AssignedBuildingIndex = 3; All[1].bIsAssigned = false;    // 갓 뽑은 대기석 직원
		All[2].AssignedBuildingIndex = 7; All[2].bIsAssigned = true;     // 다른 건물
		All[3].AssignedBuildingIndex = INDEX_NONE;                       // 무소속
		int32 MemberCount = -1;
		OfficeRosterSummary::BuildForBuilding(All, 3, Lines, MemberCount);
		TestEqual(TEXT("bench member is counted with seated member"), MemberCount, 2);
		TestEqual(TEXT("seated dev counted"), Lines[1].Headcount, 1);
		TestEqual(TEXT("benched planner counted"), Lines[0].Headcount, 1);
		TestEqual(TEXT("other building excluded"), Lines[2].Headcount, 0);
		TestEqual(TEXT("unaffiliated excluded"), Lines[3].Power, (int64)0);
	}

	// 다시 부르면 이전 결과를 남기지 않는다
	OfficeRosterSummary::Build({}, Lines);
	TestEqual(TEXT("rebuild resets lines"), Lines.Num(), 6);
	TestEqual(TEXT("rebuild resets power"), Lines[1].Power, (int64)0);
	return true;
}
#endif
