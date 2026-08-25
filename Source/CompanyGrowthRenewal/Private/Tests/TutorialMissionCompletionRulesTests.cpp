#include "Misc/AutomationTest.h"
#include "Data/TutorialMissionCompletionRules.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRTutorialMissionCompletionRulesTest,
	"CGR.Mission.TutorialCompletionPolicy.NextMissionOnly",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRTutorialMissionCompletionRulesTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("보상 없는 중간 미션은 즉시 다음 미션으로 진행"),
		FTutorialMissionCompletionRules::Resolve(true),
		ETutorialMissionCompletionMode::ImmediateAdvance);
	TestEqual(TEXT("보상이 있는 마지막 미션은 수동 피날레 클레임 유지"),
		FTutorialMissionCompletionRules::Resolve(false),
		ETutorialMissionCompletionMode::FinalClaim);
	TestEqual(TEXT("진행 필수 공급이 있는 중간 미션도 UI 없이 즉시 진행"),
		FTutorialMissionCompletionRules::Resolve(true),
		ETutorialMissionCompletionMode::ImmediateAdvance);
	TestEqual(TEXT("보상도 다음 미션도 없는 잘못된 행은 종료를 조용히 건너뛰지 않음"),
		FTutorialMissionCompletionRules::Resolve(false),
		ETutorialMissionCompletionMode::FinalClaim);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
