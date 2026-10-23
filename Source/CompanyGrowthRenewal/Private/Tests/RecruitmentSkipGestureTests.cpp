#include "Misc/AutomationTest.h"
#include "Office/RecruitmentSkipGesture.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace RecruitmentSkipGestureTests
{
constexpr int32 Mouse = INDEX_NONE;
constexpr int32 PrimaryTouch = 0;
const FVector2D Origin(100.f, 100.f);

bool Tap(FRecruitmentSkipGesture& Gesture, double DownTime, double UpTime,
	const FVector2D& Position = Origin, int32 Pointer = Mouse)
{
	Gesture.Begin(DownTime, Position, Pointer);
	return Gesture.End(UpTime, Position, Pointer);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRecruitmentSkipGestureTest,
	"CGR.OfficeRecruitment.SkipGesture.InputRules",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FRecruitmentSkipGestureTest::RunTest(const FString& Parameters)
{
	{
		FRecruitmentSkipGesture Gesture;
		TestFalse(TEXT("Unarmed input is ignored"), Tap(Gesture, 1.0, 1.05));
		TestFalse(TEXT("Unarmed double tap is ignored"), Tap(Gesture, 1.2, 1.25));
		Gesture.Arm(2.0);
		TestFalse(TEXT("Hire-button tap during guard is ignored"), Tap(Gesture, 2.0, 2.05));
		TestFalse(TEXT("Another early tap is ignored"), Tap(Gesture, 2.25, 2.30));
		TestFalse(TEXT("First eligible tap is only a candidate"), Tap(Gesture, 2.5, 2.55));
		TestTrue(TEXT("Second eligible mouse tap skips"), Tap(Gesture, 2.7, 2.75));
	}
	{
		FRecruitmentSkipGesture Gesture;
		Gesture.Arm(0.0);
		TestFalse(TEXT("First primary touch tap is only a candidate"), Tap(Gesture, 0.5, 0.55, Origin, PrimaryTouch));
		TestTrue(TEXT("Second primary touch tap skips"), Tap(Gesture, 0.7, 0.75, FVector2D(125.f, 100.f), PrimaryTouch));
		TestFalse(TEXT("Skip fires only once"), Tap(Gesture, 0.9, 0.95, Origin, PrimaryTouch));
		TestFalse(TEXT("Continued spam still cannot skip"), Tap(Gesture, 1.0, 1.05, Origin, PrimaryTouch));
		Gesture.Reset();
		TestFalse(TEXT("Reset disarms gesture"), Tap(Gesture, 1.2, 1.25, Origin, PrimaryTouch));
		Gesture.Arm(2.0);
		TestFalse(TEXT("Rearmed first tap"), Tap(Gesture, 2.5, 2.55, Origin, PrimaryTouch));
		TestTrue(TEXT("Rearmed second tap skips"), Tap(Gesture, 2.7, 2.75, Origin, PrimaryTouch));
	}
	{
		FRecruitmentSkipGesture Gesture;
		Gesture.Arm(0.0);
		TestFalse(TEXT("First tap before expiry"), Tap(Gesture, 0.5, 0.55));
		TestFalse(TEXT("Second release after 0.35 second window expires"), Tap(Gesture, 0.85, 0.91));
		TestTrue(TEXT("Expired tap becomes first of a new pair"), Tap(Gesture, 1.0, 1.05));
	}
	{
		FRecruitmentSkipGesture Gesture;
		Gesture.Arm(0.0);
		TestFalse(TEXT("First tap"), Tap(Gesture, 0.5, 0.55));
		TestFalse(TEXT("Long second press cannot skip"), Tap(Gesture, 0.7, 1.06));
		TestFalse(TEXT("Long press clears previous candidate"), Tap(Gesture, 1.1, 1.15));
		TestTrue(TEXT("Fresh pair after long press can skip"), Tap(Gesture, 1.25, 1.3));
	}
	{
		FRecruitmentSkipGesture Gesture;
		Gesture.Arm(0.0);
		TestFalse(TEXT("First tap"), Tap(Gesture, 0.5, 0.55));
		Gesture.Begin(0.7, Origin, Mouse);
		Gesture.Move(FVector2D(140.f, 100.f), Mouse);
		Gesture.Move(Origin, Mouse);
		TestFalse(TEXT("Moving back after a drag cannot restore the tap"), Gesture.End(0.75, Origin, Mouse));
		TestFalse(TEXT("Drag clears first candidate"), Tap(Gesture, 0.8, 0.85));
		TestTrue(TEXT("New pair after drag can skip"), Tap(Gesture, 0.95, 1.0));
	}
	{
		FRecruitmentSkipGesture Gesture;
		Gesture.Arm(0.0);
		TestFalse(TEXT("First tap"), Tap(Gesture, 0.5, 0.55));
		TestFalse(TEXT("Far second tap cannot skip"), Tap(Gesture, 0.7, 0.75, FVector2D(133.f, 100.f)));
		TestTrue(TEXT("Far tap starts a new pair at its own position"), Tap(Gesture, 0.85, 0.9, FVector2D(133.f, 100.f)));
	}
	{
		FRecruitmentSkipGesture Gesture;
		Gesture.Arm(0.0);
		TestFalse(TEXT("First touch tap"), Tap(Gesture, 0.5, 0.55, Origin, PrimaryTouch));
		Gesture.Begin(0.7, Origin, PrimaryTouch);
		Gesture.Begin(0.71, Origin, 1);
		TestFalse(TEXT("Second finger cannot finish primary tap"), Gesture.End(0.75, Origin, PrimaryTouch));
		TestFalse(TEXT("Secondary finger release is not a tap"), Gesture.End(0.76, Origin, 1));
		TestFalse(TEXT("Two fingers clear previous candidate"), Tap(Gesture, 0.8, 0.85, Origin, PrimaryTouch));
		TestTrue(TEXT("Fresh primary touch pair can skip"), Tap(Gesture, 0.95, 1.0, Origin, PrimaryTouch));
	}
	{
		FRecruitmentSkipGesture Gesture;
		Gesture.Arm(0.0);
		Gesture.Begin(0.5, Origin, 1);
		TestFalse(TEXT("Primary tap while secondary finger stays down"), Tap(Gesture, 0.6, 0.65, Origin, PrimaryTouch));
		TestFalse(TEXT("Repeated primary tap while secondary finger stays down"), Tap(Gesture, 0.75, 0.8, Origin, PrimaryTouch));
		TestFalse(TEXT("Third primary tap while secondary finger stays down"), Tap(Gesture, 0.9, 0.95, Origin, PrimaryTouch));
		TestFalse(TEXT("Secondary release is not a tap"), Gesture.End(1.0, Origin, 1));
		TestFalse(TEXT("First clean tap after secondary release"), Tap(Gesture, 1.1, 1.15, Origin, PrimaryTouch));
		TestTrue(TEXT("Second clean tap after secondary release skips"), Tap(Gesture, 1.25, 1.3, Origin, PrimaryTouch));
	}
	{
		FRecruitmentSkipGesture Gesture;
		Gesture.Arm(0.0);
		TestFalse(TEXT("First mouse tap"), Tap(Gesture, 0.5, 0.55));
		TestFalse(TEXT("Touch does not pair with mouse"), Tap(Gesture, 0.7, 0.75, Origin, PrimaryTouch));
		TestFalse(TEXT("Returning to mouse does not use stale mouse candidate"), Tap(Gesture, 0.8, 0.85));
		TestTrue(TEXT("New mouse pair can skip"), Tap(Gesture, 0.95, 1.0));
	}
	{
		FRecruitmentSkipGesture Gesture;
		Gesture.Arm(0.0);
		TestFalse(TEXT("First tap"), Tap(Gesture, 0.5, 0.55));
		Gesture.Begin(0.7, Origin, Mouse);
		Gesture.Arm(0.7);
		TestFalse(TEXT("Early guard rearm rejects old in-flight press"), Gesture.End(0.75, Origin, Mouse));
		TestFalse(TEXT("Rearm guard rejects new early input"), Tap(Gesture, 1.0, 1.05));
		TestFalse(TEXT("First input after rearm guard"), Tap(Gesture, 1.2, 1.25));
		TestTrue(TEXT("Second input after rearm guard"), Tap(Gesture, 1.4, 1.45));
	}
	return true;
}

} // namespace RecruitmentSkipGestureTests

#endif // WITH_DEV_AUTOMATION_TESTS
