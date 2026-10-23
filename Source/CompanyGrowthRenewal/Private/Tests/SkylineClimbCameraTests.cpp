#include "Misc/AutomationTest.h"
#include "Player/OfficeCameraPawn.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRSkylineClimbCameraTest,
	"CGR.SideContent.Skyline.ClimbCamera",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRSkylineClimbCameraTest::RunTest(const FString& Parameters)
{
	// 0층이면 오프셋 없음 — 등반 전 초기 프레이밍을 건드리지 않는다
	const FSkylineClimbCameraOffset Zero = FSkylineClimbCameraOffset::Compute(0);
	TestTrue(TEXT("0층 상승 0"), FMath::IsNearlyZero(Zero.LiftCm));
	TestTrue(TEXT("0층 후퇴 0"), FMath::IsNearlyZero(Zero.PullBackCm));

	// 층마다 는다
	const FSkylineClimbCameraOffset One = FSkylineClimbCameraOffset::Compute(1);
	const FSkylineClimbCameraOffset Five = FSkylineClimbCameraOffset::Compute(5);
	TestTrue(TEXT("층 수에 따라 상승"), Five.LiftCm > One.LiftCm);
	TestTrue(TEXT("층 수에 따라 후퇴"), Five.PullBackCm > One.PullBackCm);

	// 상한에서 멈춘다 — 100층 등반에도 카메라가 성층권으로 가지 않는다
	const FSkylineClimbCameraOffset Deep = FSkylineClimbCameraOffset::Compute(100);
	TestTrue(TEXT("상승 상한"), Deep.LiftCm <= FSkylineClimbCameraOffset::MaxLiftCm + KINDA_SMALL_NUMBER);
	TestTrue(TEXT("후퇴 상한"), Deep.PullBackCm <= FSkylineClimbCameraOffset::MaxPullBackCm + KINDA_SMALL_NUMBER);

	// 음수 입력도 0 으로 물린다
	const FSkylineClimbCameraOffset Negative = FSkylineClimbCameraOffset::Compute(-3);
	TestTrue(TEXT("음수는 0"), FMath::IsNearlyZero(Negative.LiftCm));
	TestTrue(TEXT("음수 후퇴도 0"), FMath::IsNearlyZero(Negative.PullBackCm));

	// 한 층당 증가분이 계약대로다
	TestTrue(TEXT("1층 상승 = LiftPerFloorCm"),
		FMath::IsNearlyEqual(One.LiftCm, FSkylineClimbCameraOffset::LiftPerFloorCm));

	return true;
}

#endif
