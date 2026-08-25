#include "Misc/AutomationTest.h"
#include "UI/Element/Common/GestureHintTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGestureHintMotionTest,
	"CGR.UI.GestureHintMotion",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGestureHintMotionTest::RunTest(const FString& Parameters)
{
	using namespace GestureHintMotion;
	TestNearlyEqual(TEXT("tap rest at 0"), TapOffsetY(0.f), 0.f, 0.01f);
	TestNearlyEqual(TEXT("tap dip at 35%"), TapOffsetY(TapCycle * 0.35f), TapDipPx, 0.05f);
	TestNearlyEqual(TEXT("tap holds dip at 50%"), TapOffsetY(TapCycle * 0.50f), TapDipPx, 0.01f);
	TestNearlyEqual(TEXT("tap back at cycle end"), TapOffsetY(TapCycle * 0.999f), 0.f, 0.1f);
	TestNearlyEqual(TEXT("tap wraps"), TapOffsetY(TapCycle * 1.35f), TapDipPx, 0.05f);

	float S = 0.f, A = 1.f;
	TapPulse(0.f, S, A);
	TestEqual(TEXT("pulse hidden before contact"), A, 0.f);
	TapPulse(TapCycle * 0.35f, S, A);
	TestTrue(TEXT("pulse visible at contact"), A > 0.9f && S < 0.5f);
	TapPulse(TapCycle * 0.999f, S, A);
	TestTrue(TEXT("pulse faded at end"), A < 0.05f && S > 1.5f);

	TestNearlyEqual(TEXT("hold 0 at start"), HoldPercent(0.f), 0.f, 0.001f);
	TestNearlyEqual(TEXT("hold half at 49%"), HoldPercent(HoldCycle * 0.49f), 0.5f, 0.02f);
	TestNearlyEqual(TEXT("hold full at 90%"), HoldPercent(HoldCycle * 0.90f), 1.f, 0.001f);
	TestNearlyEqual(TEXT("hold reset at 97%"), HoldPercent(HoldCycle * 0.97f), 0.f, 0.001f);
	TestNearlyEqual(TEXT("hold scale pressed mid"), HoldScale(HoldCycle * 0.5f), HoldPressScale, 0.001f);
	TestNearlyEqual(TEXT("hold scale released start"), HoldScale(0.f), 1.f, 0.001f);

	TestNearlyEqual(TEXT("drag left at 0"), DragOffsetX(0.f), -DragAmpPx, 0.01f);
	TestNearlyEqual(TEXT("drag right at half"), DragOffsetX(DragCycle * 0.5f), DragAmpPx, 0.01f);
	TestNearlyEqual(TEXT("drag center at quarter"), DragOffsetX(DragCycle * 0.25f), 0.f, 0.01f);
	TestNearlyEqual(TEXT("negative time clamps"), DragOffsetX(-5.f), -DragAmpPx, 0.01f);
	return true;
}

#endif
