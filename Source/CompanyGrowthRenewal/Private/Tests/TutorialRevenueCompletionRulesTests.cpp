#include "Misc/AutomationTest.h"
#include "Data/TutorialRevenueCompletionRules.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRTutorialRevenueCompletionRulesTest,
	"CGR.Mission.TutorialRevenueCompletionRules",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRTutorialRevenueCompletionRulesTest::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("Negative collection never completes the tutorial revenue mission"),
		FTutorialRevenueCompletionRules::IsPositiveCollection(-1));
	TestFalse(TEXT("Zero collection never completes the tutorial revenue mission"),
		FTutorialRevenueCompletionRules::IsPositiveCollection(0));
	TestTrue(TEXT("The first positive collection completes the tutorial revenue mission"),
		FTutorialRevenueCompletionRules::IsPositiveCollection(1));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
