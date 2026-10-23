#include "Misc/AutomationTest.h"
#include "Data/ShippedRecord.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShippedProjectBuildingIsolationTest,
	"CGR.Production.ShippedProject.BuildingIsolation",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FShippedProjectBuildingIsolationTest::RunTest(const FString& Parameters)
{
	TArray<FShippedProjectRecord> Records;
	FShippedProjectRecord& First = Records.AddDefaulted_GetRef();
	First.BuildingIndex = 4;
	First.Industry = ECompanyType::Game;
	First.ProjectIndex = 3;
	First.ReviewScore = 22;

	TestNull(TEXT("같은 산업·프로젝트라도 다른 건물에는 출시 기록이 없다"),
		ShippedProjectRecords::Find(Records, 7, ECompanyType::Game, 3));

	FShippedProjectRecord& Second = Records.AddDefaulted_GetRef();
	Second.BuildingIndex = 7;
	Second.Industry = ECompanyType::Game;
	Second.ProjectIndex = 3;
	Second.ReviewScore = 31;

	FShippedProjectRecord* FirstResult = ShippedProjectRecords::Find(Records, 4, ECompanyType::Game, 3);
	FShippedProjectRecord* SecondResult = ShippedProjectRecords::Find(Records, 7, ECompanyType::Game, 3);
	if (!TestNotNull(TEXT("첫 건물의 출시 기록"), FirstResult)
		|| !TestNotNull(TEXT("둘째 건물의 출시 기록"), SecondResult))
	{
		return false;
	}
	TestEqual(TEXT("첫 건물 평점"), FirstResult->ReviewScore, 22);
	TestEqual(TEXT("둘째 건물 평점"), SecondResult->ReviewScore, 31);
	FirstResult->CumulativeRevenue += 1000;
	TestEqual(TEXT("첫 건물 매출만 누적"), FirstResult->CumulativeRevenue, int64(1000));
	TestEqual(TEXT("둘째 건물 매출은 독립"), SecondResult->CumulativeRevenue, int64(0));
	TestNull(TEXT("다른 산업은 같은 건물·프로젝트를 공유하지 않는다"),
		ShippedProjectRecords::Find(Records, 4, ECompanyType::IT, 3));
	return true;
}
#endif
