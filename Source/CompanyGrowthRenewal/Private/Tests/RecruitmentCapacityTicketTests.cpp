#include "Misc/AutomationTest.h"
#include "Data/RecruitmentCapacityTicketRules.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRRecruitmentCapacityTicketRulesTest,
	"CGR.Recruitment.CapacityTicketRules",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRRecruitmentCapacityTicketRulesTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("첫 1칸 회사는 정원 2만큼 지급"),
		FRecruitmentCapacityTicketRules::Evaluate(2, 0).GrantCount, 2);
	TestEqual(TEXT("증축은 증가분만 지급"),
		FRecruitmentCapacityTicketRules::Evaluate(3, 2).GrantCount, 1);
	TestEqual(TEXT("같은 정원 재평가는 멱등"),
		FRecruitmentCapacityTicketRules::Evaluate(3, 3).GrantCount, 0);
	TestEqual(TEXT("철거로 현재 정원이 줄어도 최고치는 유지"),
		FRecruitmentCapacityTicketRules::Evaluate(1, 3).NewCreditedCapacity, 3);
	TestEqual(TEXT("과거 최고치까지 재건해도 중복 지급 없음"),
		FRecruitmentCapacityTicketRules::Evaluate(3, 3).GrantCount, 0);
	TestEqual(TEXT("과거 최고치를 넘은 증가분만 지급"),
		FRecruitmentCapacityTicketRules::Evaluate(4, 3).GrantCount, 1);
	TestEqual(TEXT("음수 현재 정원은 0으로 정규화"),
		FRecruitmentCapacityTicketRules::Evaluate(-4, 2).GrantCount, 0);
	TestEqual(TEXT("음수 인정 최고치는 0으로 정규화"),
		FRecruitmentCapacityTicketRules::Evaluate(2, -7).NewCreditedCapacity, 2);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
