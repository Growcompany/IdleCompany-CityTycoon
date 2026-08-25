#include "Misc/AutomationTest.h"
#include "Utils/FWidgetAnimationUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRAnimUtilsStampTest,
	"CGR.Launch.AnimUtils.Stamp",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRAnimUtilsStampTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("시작 스케일 2.8"), FWidgetAnimationUtils::StampInScale(0.f), 2.8f);
	TestTrue(TEXT("0.7 지점 = 0.94 (착지)"), FMath::IsNearlyEqual(FWidgetAnimationUtils::StampInScale(0.7f), 0.94f, 0.001f));
	TestEqual(TEXT("종료 스케일 1.0"), FWidgetAnimationUtils::StampInScale(1.f), 1.f);
	TestTrue(TEXT("단조 감소(0→0.7)"), FWidgetAnimationUtils::StampInScale(0.3f) > FWidgetAnimationUtils::StampInScale(0.5f));
	TestTrue(TEXT("범위 밖 입력 클램프"), FMath::IsNearlyEqual(FWidgetAnimationUtils::StampInScale(2.f), 1.f));
	TestEqual(TEXT("불투명도 0"), FWidgetAnimationUtils::StampInOpacity(0.f), 0.f);
	TestTrue(TEXT("불투명도 상한 0.94"), FMath::IsNearlyEqual(FWidgetAnimationUtils::StampInOpacity(0.5f), 0.94f));
	TestTrue(TEXT("상한 인자 존중"), FMath::IsNearlyEqual(FWidgetAnimationUtils::StampInOpacity(1.f, 0.5f), 0.5f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRAnimUtilsShakeTest,
	"CGR.Launch.AnimUtils.Shake",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRAnimUtilsShakeTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("시작 전 = 0"), FWidgetAnimationUtils::ShakeOffset(-0.1f, 0.2f, 10.f).IsNearlyZero());
	TestTrue(TEXT("종료 후 = 0"), FWidgetAnimationUtils::ShakeOffset(0.25f, 0.2f, 10.f).IsNearlyZero());
	for (float E = 0.f; E < 0.2f; E += 0.01f)
	{
		const FVector2D O = FWidgetAnimationUtils::ShakeOffset(E, 0.2f, 10.f);
		TestTrue(TEXT("진폭 상한"), FMath::Abs(O.X) <= 10.f && FMath::Abs(O.Y) <= 10.f);
	}
	// 구간 안에서 실제로 흔들려야 한다 — 이 단언이 없으면 ZeroVector 만 뱉는 구현도 나머지를 전부 통과한다
	const float StartSize = FWidgetAnimationUtils::ShakeOffset(0.01f, 0.2f, 10.f).Size();
	const float EndSize = FWidgetAnimationUtils::ShakeOffset(0.19f, 0.2f, 10.f).Size();
	TestTrue(TEXT("셰이크가 실제로 움직인다"), StartSize > 1.f);
	TestTrue(TEXT("감쇠: 끝 직전이 시작 직후보다 작다"), EndSize < StartSize);
	TestTrue(TEXT("끝 직전은 거의 0"), EndSize < 1.f);
	TestTrue(TEXT("Duration 0 방어"), FWidgetAnimationUtils::ShakeOffset(0.f, 0.f, 10.f).IsNearlyZero());
	return true;
}

#endif
