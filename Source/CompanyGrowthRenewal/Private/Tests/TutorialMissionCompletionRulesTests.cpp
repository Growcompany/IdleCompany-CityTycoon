#include "Misc/AutomationTest.h"
#include "Data/TutorialMissionCompletionRules.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRTutorialMissionCompletionRulesTest,
	"CGR.Mission.TutorialCompletionPolicy.ManualClaim",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRTutorialMissionCompletionRulesTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Final M7 pauses for explicit reward claim"),
		FTutorialMissionCompletionRules::Resolve(true), ETutorialMissionCompletionMode::ManualClaim);
	TestEqual(TEXT("A nonmanual mission advances without a claim"),
		FTutorialMissionCompletionRules::Resolve(false), ETutorialMissionCompletionMode::ImmediateAdvance);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
