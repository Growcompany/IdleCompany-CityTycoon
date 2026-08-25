#include "Misc/AutomationTest.h"
#include "Data/StageProgressData.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRStageGoalMetTest,
	"CGR.Office.StageGoalMet",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRStageGoalMetTest::RunTest(const FString& Parameters)
{
	// 문턱은 1.0 이 아니라 0.999 ― 카운트업이 PctEps(상한 0.001) 안에 들면 조기 이탈해 Target 에 못 닿는다.
	// 근거 상세는 IsStageGoalMet 선언부 주석(StageProgressData.h)이 SOT.
	TestFalse(TEXT("0.0 은 미달"), IsStageGoalMet(0.0f));
	TestFalse(TEXT("0.9 는 미달"), IsStageGoalMet(0.9f));
	TestFalse(TEXT("0.9989 는 미달"), IsStageGoalMet(0.9989f));
	TestTrue(TEXT("0.999 는 달성"), IsStageGoalMet(0.999f));
	TestTrue(TEXT("1.0 은 달성"), IsStageGoalMet(1.0f));

	// 오버필도 달성이다 ― 문턱 함수가 목표 초과를 계속 "달성"으로 친다는 계약.
	// ⚠ 이 테스트가 실행하는 것은 IsStageGoalMet 뿐이다. 호출부가 오버필을 따로 빼는 회귀
	//   (예: Done->SetVisibility(bFull && !bOver ...))는 여기서 안 잡힌다.
	TestTrue(TEXT("1.5 오버필도 달성"), IsStageGoalMet(1.5f));

	return true;
}

#endif
