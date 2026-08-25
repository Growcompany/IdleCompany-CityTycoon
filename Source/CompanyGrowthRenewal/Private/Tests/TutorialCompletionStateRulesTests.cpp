#include "Misc/AutomationTest.h"
#include "Data/TutorialCompletionStateRules.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRTutorialCompletionStateRulesTest,
	"CGR.Mission.TutorialCompletionState",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRTutorialCompletionStateRulesTest::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("신규 정식 게임은 튜토리얼 미완료"),
		FTutorialCompletionStateRules::ResolveOnLoad(
			/*bHasSave=*/false,
			/*bSavedCompleted=*/false,
			/*bStartsWithoutTutorial=*/false,
			/*bDevJumpToMission=*/false));
	TestTrue(TEXT("완료 세이브는 명시 상태를 복원"),
		FTutorialCompletionStateRules::ResolveOnLoad(
			/*bHasSave=*/true,
			/*bSavedCompleted=*/true,
			/*bStartsWithoutTutorial=*/false,
			/*bDevJumpToMission=*/false));
	TestFalse(TEXT("GoalBoard 언락은 튜토리얼 완료의 proxy가 아님"),
		FTutorialCompletionStateRules::ResolveOnLoad(
			/*bHasSave=*/true,
			/*bSavedCompleted=*/false,
			/*bStartsWithoutTutorial=*/false,
			/*bDevJumpToMission=*/false));
	TestTrue(TEXT("Skip/Sandbox/MidGame 시작은 일반 강화가 가능한 완료 상태"),
		FTutorialCompletionStateRules::ResolveOnLoad(
			/*bHasSave=*/false,
			/*bSavedCompleted=*/false,
			/*bStartsWithoutTutorial=*/true,
			/*bDevJumpToMission=*/false));
	TestFalse(TEXT("명시 미션 점프는 다시 튜토리얼 미완료 상태"),
		FTutorialCompletionStateRules::ResolveOnLoad(
			/*bHasSave=*/true,
			/*bSavedCompleted=*/true,
			/*bStartsWithoutTutorial=*/true,
			/*bDevJumpToMission=*/true));

	TestFalse(TEXT("완료 세이브는 건물이 없어도 오프닝을 다시 시작하지 않음"),
		FTutorialCompletionStateRules::ShouldRestoreOpeningChain(
			/*bHasSave=*/true,
			/*SavedMissionID=*/NAME_None,
			/*SavedBuildingCount=*/0,
			/*bSavedTutorialCompleted=*/true));
	TestTrue(TEXT("미완료 레거시 세이브의 None과 건물 0채는 오프닝으로 복구"),
		FTutorialCompletionStateRules::ShouldRestoreOpeningChain(
			/*bHasSave=*/true,
			/*SavedMissionID=*/NAME_None,
			/*SavedBuildingCount=*/0,
			/*bSavedTutorialCompleted=*/false));
	TestFalse(TEXT("진행 중 미션이 있는 세이브는 오프닝 복구 대상이 아님"),
		FTutorialCompletionStateRules::ShouldRestoreOpeningChain(
			/*bHasSave=*/true,
			/*SavedMissionID=*/TEXT("M4_PlaceDesks"),
			/*SavedBuildingCount=*/0,
			/*bSavedTutorialCompleted=*/false));

	const bool bFirstJumpLatch = FTutorialCompletionStateRules::ResolveDevJumpSessionActive(
		/*bWasActive=*/false,
		/*bJumpApplied=*/true,
		/*bReachedTerminalCompletion=*/false);
	TestTrue(TEXT("비-FullTutorial 모드의 첫 dev jump가 세션 래치를 설정"), bFirstJumpLatch);
	TestFalse(TEXT("첫 dev jump는 Skip 계열 시작 모드보다 우선해 미완료"),
		FTutorialCompletionStateRules::ResolveOnLoad(
			/*bHasSave=*/true,
			/*bSavedCompleted=*/true,
			/*bStartsWithoutTutorial=*/true,
			/*bDevJumpSessionActive=*/bFirstJumpLatch));

	const bool bSecondMapLatch = FTutorialCompletionStateRules::ResolveDevJumpSessionActive(
		/*bWasActive=*/bFirstJumpLatch,
		/*bJumpApplied=*/false,
		/*bReachedTerminalCompletion=*/false);
	TestTrue(TEXT("dev jump 세션 래치는 두 번째 맵 로드에서도 유지"), bSecondMapLatch);
	TestFalse(TEXT("두 번째 맵에서도 Money 강화가 풀리지 않음"),
		FTutorialCompletionStateRules::ResolveOnLoad(
			/*bHasSave=*/true,
			/*bSavedCompleted=*/false,
			/*bStartsWithoutTutorial=*/true,
			/*bDevJumpSessionActive=*/bSecondMapLatch));

	TestFalse(TEXT("피날레 완료는 dev jump 세션 래치를 해제"),
		FTutorialCompletionStateRules::ResolveDevJumpSessionActive(
			/*bWasActive=*/bSecondMapLatch,
			/*bJumpApplied=*/false,
			/*bReachedTerminalCompletion=*/true));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
