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
	TestTrue(TEXT("Positive company revenue is recorded"),
		FTutorialRevenueCompletionRules::IsPositiveCollection(1));
	TestFalse(TEXT("Income arriving during explanation does not skip it"),
		FTutorialRevenueCompletionRules::CanFinish(true, false, 10.0));
	TestFalse(TEXT("Finishing explanation without actual income is insufficient"),
		FTutorialRevenueCompletionRules::CanFinish(false, true, 10.0));
	TestFalse(TEXT("The final explanation tap leaves time to observe income"),
		FTutorialRevenueCompletionRules::CanFinish(true, true, 0.0));
	TestTrue(TEXT("Previously received income completes after explanation and observation"),
		FTutorialRevenueCompletionRules::CanFinish(true, true, 1.5));
	TestTrue(TEXT("Receipt survives restarting the same mission"),
		FTutorialRevenueCompletionRules::RestoreReceipt(true, TEXT("M7"), TEXT("M7"), true));
	TestFalse(TEXT("Another mission cannot inherit a receipt"),
		FTutorialRevenueCompletionRules::RestoreReceipt(true, TEXT("M7"), TEXT("M6"), false));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
