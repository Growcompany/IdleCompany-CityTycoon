#include "Misc/AutomationTest.h"
#include "Data/WorkstationCapacityRules.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRWorkstationCapacityRulesTest,
	"CGR.Office.WorkstationCapacityRules",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRWorkstationCapacityRulesTest::RunTest(const FString& Parameters)
{
	const FWorkstationCapacityDecision UnderCapacity = FWorkstationCapacityRules::Evaluate(1, 1, 3);
	TestTrue(TEXT("정원 미만 배치는 허용한다"), UnderCapacity.bCanPlace);
	TestFalse(TEXT("정원 미만 배치는 자동 종료하지 않는다"), UnderCapacity.bReachesCapacity);

	const FWorkstationCapacityDecision ExactCapacity = FWorkstationCapacityRules::Evaluate(1, 2, 3);
	TestTrue(TEXT("1인 좌석 뒤 2인 좌석으로 정원에 정확히 도달할 수 있다"), ExactCapacity.bCanPlace);
	TestTrue(TEXT("정원 정확 도달은 자동 종료를 요청한다"), ExactCapacity.bReachesCapacity);

	const FWorkstationCapacityDecision OverCapacity = FWorkstationCapacityRules::Evaluate(2, 2, 3);
	TestFalse(TEXT("남은 좌석보다 큰 후보는 거부한다"), OverCapacity.bCanPlace);
	TestFalse(TEXT("초과 후보는 자동 종료하지 않는다"), OverCapacity.bReachesCapacity);

	TestFalse(TEXT("이미 만석이면 추가 배치를 거부한다"),
		FWorkstationCapacityRules::Evaluate(3, 1, 3).bCanPlace);
	TestFalse(TEXT("이미 정원을 초과한 상태면 추가 배치를 거부한다"),
		FWorkstationCapacityRules::Evaluate(4, 1, 3).bCanPlace);
	TestFalse(TEXT("정원 0은 배치를 거부한다"),
		FWorkstationCapacityRules::Evaluate(0, 1, 0).bCanPlace);
	TestFalse(TEXT("음수 정원은 배치를 거부한다"),
		FWorkstationCapacityRules::Evaluate(0, 1, -1).bCanPlace);
	TestFalse(TEXT("후보 좌석 0은 배치를 거부한다"),
		FWorkstationCapacityRules::Evaluate(0, 0, 3).bCanPlace);
	TestFalse(TEXT("음수 후보 좌석은 배치를 거부한다"),
		FWorkstationCapacityRules::Evaluate(0, -1, 3).bCanPlace);
	TestFalse(TEXT("음수 현재 좌석은 배치를 거부한다"),
		FWorkstationCapacityRules::Evaluate(-1, 1, 3).bCanPlace);

	TestEqual(TEXT("2인 책상은 미션 좌석 진행도를 2 올린다"),
		FWorkstationMissionProgressRules::GetProgressIncrement(2), 2);
	TestEqual(TEXT("기존 인자 없는 호출은 미션 진행도를 최소 1 올린다"),
		FWorkstationMissionProgressRules::GetProgressIncrement(0), 1);
	TestEqual(TEXT("잘못된 음수 카운트도 미션 진행도를 최소 1 올린다"),
		FWorkstationMissionProgressRules::GetProgressIncrement(-2), 1);

	const FName DeskMission(TEXT("M4_PlaceDesk"));
	TestEqual(TEXT("The same PlaceDesks mission restores its saved partial seat progress"),
		FWorkstationMissionProgressRules::ResolveRestoredProgress(
			DeskMission, DeskMission, true, 1, 2), 1);
	TestEqual(TEXT("Progress from another mission is stale and ignored"),
		FWorkstationMissionProgressRules::ResolveRestoredProgress(
			TEXT("M3_EnterOffice"), DeskMission, true, 1, 2), 0);
	TestEqual(TEXT("Progress is ignored when the restored condition is not PlaceDesks"),
		FWorkstationMissionProgressRules::ResolveRestoredProgress(
			DeskMission, DeskMission, false, 1, 2), 0);
	TestEqual(TEXT("Restored progress is clamped to the mission target"),
		FWorkstationMissionProgressRules::ResolveRestoredProgress(
			DeskMission, DeskMission, true, 5, 2), 2);
	TestEqual(TEXT("Negative restored progress is clamped to zero"),
		FWorkstationMissionProgressRules::ResolveRestoredProgress(
			DeskMission, DeskMission, true, -1, 2), 0);

	TestFalse(TEXT("Immediate mission completion already saved the placed desk and next mission"),
		FWorkstationMissionProgressRules::ShouldSaveAfterPlacement(
			/*bWasPlaceDesksMission=*/true,
			/*bIsPlaceDesksMission=*/false));
	TestTrue(TEXT("Partial PlaceDesks progress needs one placement snapshot save"),
		FWorkstationMissionProgressRules::ShouldSaveAfterPlacement(
			/*bWasPlaceDesksMission=*/true,
			/*bIsPlaceDesksMission=*/true));
	TestTrue(TEXT("Free placement outside the tutorial still persists its workstation"),
		FWorkstationMissionProgressRules::ShouldSaveAfterPlacement(
			/*bWasPlaceDesksMission=*/false,
			/*bIsPlaceDesksMission=*/false));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
