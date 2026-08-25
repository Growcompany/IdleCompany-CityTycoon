#include "Misc/AutomationTest.h"
#include "Data/RecruitEmployeesMissionRules.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRRecruitEmployeesMissionRulesTest,
	"CGR.Mission.RecruitEmployees.ReachRule",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRRecruitEmployeesMissionRulesTest::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("직원 1명은 목표 2명에 미달"),
		FRecruitEmployeesMissionRules::HasReachedTarget(1, 2));
	TestTrue(TEXT("직원 2명은 착석 여부와 무관하게 목표 도달"),
		FRecruitEmployeesMissionRules::HasReachedTarget(2, 2));
	TestTrue(TEXT("목표를 초과해도 도달 상태 유지"),
		FRecruitEmployeesMissionRules::HasReachedTarget(3, 2));
	TestTrue(TEXT("잘못된 0 목표는 최소 1명으로 정규화"),
		FRecruitEmployeesMissionRules::HasReachedTarget(1, 0));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
