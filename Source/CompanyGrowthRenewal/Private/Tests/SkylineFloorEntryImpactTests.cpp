#include "Misc/AutomationTest.h"
#include "UI/Element/Skyline/SkylineFloorEntryWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRSkylineSlamCurveTest,
	"CGR.SideContent.Skyline.SlamCurve",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRSkylineSlamCurveTest::RunTest(const FString& Parameters)
{
	// 시작은 위쪽(음수 Y)에서 떨어진다
	TestTrue(TEXT("시작 오프셋은 위쪽"), USkylineFloorEntryWidget::GetSlamOffsetY(0.0f) < -1.0f);

	// 끝은 제자리다
	TestTrue(TEXT("끝 오프셋 0"), FMath::IsNearlyZero(USkylineFloorEntryWidget::GetSlamOffsetY(1.0f), 0.01f));

	// 단조 증가 — 위(음수)에서 제자리(0)로 내려오며 되올라가지 않는다
	float Previous = USkylineFloorEntryWidget::GetSlamOffsetY(0.0f);
	for (int32 Step = 1; Step <= 20; ++Step)
	{
		const float Current = USkylineFloorEntryWidget::GetSlamOffsetY(Step / 20.0f);
		TestTrue(TEXT("낙하는 단조롭다"), Current >= Previous - KINDA_SMALL_NUMBER);
		Previous = Current;
	}

	// 범위 밖은 0 으로 물린다 — 애니가 끝난 뒤 위젯이 어긋난 자리에 남으면 안 된다
	TestTrue(TEXT("1 초과는 0"), FMath::IsNearlyZero(USkylineFloorEntryWidget::GetSlamOffsetY(1.6f), 0.01f));

	// 음수 진행도는 시작 위치로 물린다
	TestTrue(TEXT("음수는 시작 위치"),
		FMath::IsNearlyEqual(USkylineFloorEntryWidget::GetSlamOffsetY(-0.5f),
			USkylineFloorEntryWidget::GetSlamOffsetY(0.0f), 0.01f));

	return true;
}

#endif
