#include "Misc/AutomationTest.h"
#include "Data/CountedGoalProgressRules.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGoalBoardCountedEventProgressTest,
	"CGR.GoalBoard.CountedEventProgress",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGoalBoardCountedEventProgressTest::RunTest(const FString& Parameters)
{
	const FCountedGoalProgressDecision First = FCountedGoalProgressRules::ApplySignal(0, 5, false);
	TestEqual(TEXT("0/5에서 한 번의 신호는 1/5가 된다"), First.Current, int64{1});
	TestEqual(TEXT("설정된 목표 5를 유지한다"), First.Target, int64{5});
	TestTrue(TEXT("첫 번째 신호는 진행도를 변경한다"), First.bChanged);
	TestFalse(TEXT("1/5는 목표에 도달하지 않는다"), First.bReachedThisSignal);

	const FCountedGoalProgressDecision Fifth = FCountedGoalProgressRules::ApplySignal(4, 5, false);
	TestEqual(TEXT("4/5에서 다섯 번째 신호는 5/5가 된다"), Fifth.Current, int64{5});
	TestTrue(TEXT("다섯 번째 신호는 진행도를 변경한다"), Fifth.bChanged);
	TestTrue(TEXT("다섯 번째 신호만 목표 도달을 알린다"), Fifth.bReachedThisSignal);

	const FCountedGoalProgressDecision AfterTarget = FCountedGoalProgressRules::ApplySignal(5, 5, false);
	TestEqual(TEXT("이미 5/5이면 추가 신호에도 5/5를 유지한다"), AfterTarget.Current, int64{5});
	TestFalse(TEXT("이미 목표에 도달했으면 변경하지 않는다"), AfterTarget.bChanged);
	TestFalse(TEXT("여섯 번째 신호는 새 목표 도달이 아니다"), AfterTarget.bReachedThisSignal);

	const FCountedGoalProgressDecision AlreadyCompleted = FCountedGoalProgressRules::ApplySignal(3, 5, true);
	TestEqual(TEXT("완료 래치된 목표는 저장 진행도 3/5를 변경하지 않는다"), AlreadyCompleted.Current, int64{3});
	TestFalse(TEXT("완료 래치된 목표는 추가 신호를 무시한다"), AlreadyCompleted.bChanged);
	TestFalse(TEXT("완료 래치된 목표는 다시 도달하지 않는다"), AlreadyCompleted.bReachedThisSignal);

	const FCountedGoalProgressDecision ZeroTarget = FCountedGoalProgressRules::ApplySignal(0, 0, false);
	TestEqual(TEXT("목표 0은 최소 목표 1로 정규화한다"), ZeroTarget.Target, int64{1});
	TestEqual(TEXT("정규화된 1회 목표는 첫 신호에서 1/1이 된다"), ZeroTarget.Current, int64{1});
	TestTrue(TEXT("정규화된 1회 목표는 첫 신호에서 도달한다"), ZeroTarget.bReachedThisSignal);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
