#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Office/OfficeManager.h"
#include "Data/EmployeeTypes.h"

namespace
{
	FEmployeeInstance MakeEmp(int32 Id, int32 WorkSpeed, int32 Enhancement)
	{
		FEmployeeInstance E;
		E.EmployeeID = Id;
		E.Stats.WorkSpeed = WorkSpeed;
		E.EnhancementLevel = Enhancement;
		return E;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBenchSortByOverallTest,
	"CGR.Office.BenchSortByOverall",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBenchSortByOverallTest::RunTest(const FString& Parameters)
{
	{	// 종합 내림차순
		TArray<FEmployeeInstance> Bench = { MakeEmp(1, 10, 0), MakeEmp(2, 50, 0), MakeEmp(3, 30, 0) };
		UOfficeManager::SortBenchByOverallDesc(Bench);
		TestEqual(TEXT("1위"), Bench[0].EmployeeID, 2);
		TestEqual(TEXT("2위"), Bench[1].EmployeeID, 3);
		TestEqual(TEXT("3위"), Bench[2].EmployeeID, 1);
	}
	{	// ★ 보너스가 반영된다 — 저장값만 보면 순서가 뒤집힌다
		TArray<FEmployeeInstance> Bench = { MakeEmp(1, 40, 0), MakeEmp(2, 30, 10) };
		UOfficeManager::SortBenchByOverallDesc(Bench);
		TestEqual(TEXT("★10 직원이 앞선다"), Bench[0].EmployeeID, 2);
	}
	{	// 동률이면 입력 순서 유지 — 8개 이하는 IntroSort 의 선택정렬 경로
		TArray<FEmployeeInstance> Bench = { MakeEmp(7, 20, 0), MakeEmp(8, 20, 0), MakeEmp(9, 20, 0) };
		UOfficeManager::SortBenchByOverallDesc(Bench);
		TestEqual(TEXT("동률 1"), Bench[0].EmployeeID, 7);
		TestEqual(TEXT("동률 2"), Bench[1].EmployeeID, 8);
		TestEqual(TEXT("동률 3"), Bench[2].EmployeeID, 9);
	}
	{	// 동률 12개 — IntroSort 는 9개부터 분할 경로라 Sort 로 바꾸면 순서가 깨진다
		TArray<FEmployeeInstance> Bench;
		for (int32 Id = 1; Id <= 12; ++Id)
		{
			Bench.Add(MakeEmp(Id, 20, 0));
		}
		UOfficeManager::SortBenchByOverallDesc(Bench);
		for (int32 Idx = 0; Idx < Bench.Num(); ++Idx)
		{
			TestEqual(FString::Printf(TEXT("동률 12개 - %d번째"), Idx), Bench[Idx].EmployeeID, Idx + 1);
		}
	}
	{	// 빈 배열에서 죽지 않는다
		TArray<FEmployeeInstance> Bench;
		UOfficeManager::SortBenchByOverallDesc(Bench);
		TestEqual(TEXT("빈 배열 유지"), Bench.Num(), 0);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
