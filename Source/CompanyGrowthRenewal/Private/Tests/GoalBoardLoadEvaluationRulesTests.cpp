#include "Misc/AutomationTest.h"
#include "Data/GoalBoardLoadEvaluationRules.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGoalBoardLoadEvaluationOrderingTest,
	"CGR.GoalBoard.LoadEvaluation.SaveBeforePublish",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGoalBoardLoadEvaluationOrderingTest::RunTest(const FString& Parameters)
{
	const FGoalBoardLoadEvaluationDecision CleanLoad =
		FGoalBoardLoadEvaluationRules::Resolve(/*bEvaluationDirty=*/false, /*bSaveAttempted=*/false, /*bSaveSucceeded=*/false);
	TestFalse(TEXT("변경 없는 로드는 후속 저장을 예약하지 않는다"), CleanLoad.bScheduleSaveForNextTick);
	TestTrue(TEXT("변경 없는 로드는 즉시 UI에 게시한다"), CleanLoad.bBroadcastBoardChanged);

	const FGoalBoardLoadEvaluationDecision DirtyLoad =
		FGoalBoardLoadEvaluationRules::Resolve(/*bEvaluationDirty=*/true, /*bSaveAttempted=*/false, /*bSaveSucceeded=*/false);
	TestTrue(TEXT("변경된 로드는 다음 틱 저장을 예약한다"), DirtyLoad.bScheduleSaveForNextTick);
	TestFalse(TEXT("변경된 로드는 저장 전에 UI에 게시하지 않는다"), DirtyLoad.bBroadcastBoardChanged);

	const FGoalBoardLoadEvaluationDecision FailedSave =
		FGoalBoardLoadEvaluationRules::Resolve(/*bEvaluationDirty=*/true, /*bSaveAttempted=*/true, /*bSaveSucceeded=*/false);
	TestFalse(TEXT("실패한 저장은 다시 같은 후속작업을 예약하지 않는다"), FailedSave.bScheduleSaveForNextTick);
	TestFalse(TEXT("저장 실패 상태는 UI에 게시하지 않는다"), FailedSave.bBroadcastBoardChanged);

	const FGoalBoardLoadEvaluationDecision SuccessfulSave =
		FGoalBoardLoadEvaluationRules::Resolve(/*bEvaluationDirty=*/true, /*bSaveAttempted=*/true, /*bSaveSucceeded=*/true);
	TestFalse(TEXT("성공한 저장은 추가 후속작업이 필요 없다"), SuccessfulSave.bScheduleSaveForNextTick);
	TestTrue(TEXT("변경 상태는 저장 성공 후에만 UI에 게시한다"), SuccessfulSave.bBroadcastBoardChanged);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
