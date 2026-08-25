#include "Misc/AutomationTest.h"
#include "Utils/MobileFacadeMaterialTuning.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRMobileFacadeQuietPlusContractTest,
	"CGR.Building.MobileFacade.QuietPlusContract",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRMobileFacadeQuietPlusContractTest::RunTest(const FString& Parameters)
{
	using namespace MobileFacadeMaterialTuning;
	const FQuietPlusTuning& Tuning = GetQuietPlusTuning();

	TestEqual(TEXT("창문 금속성"), Tuning.WindowsMetallic, 0.10f);
	TestEqual(TEXT("창문 거칠기"), Tuning.WindowsRoughness, 0.40f);
	TestEqual(TEXT("프레임 금속성"), Tuning.FramesMetallic, 0.12f);
	TestEqual(TEXT("프레임 거칠기"), Tuning.FramesRoughness, 0.56f);
	TestEqual(TEXT("공통 스페큘러"), Tuning.Specular, 0.34f);
	TestEqual(TEXT("노멀 강도"), Tuning.NormalIntensity, 0.08f);
	TestEqual(TEXT("창문은 약 15%만 점등"), Tuning.WindowLightThreshold, 0.85f);
	TestEqual(TEXT("중립 유리용 모바일 가짜 반사 강도"), Tuning.FakeReflectionStrength, 0.45f);
	TestTrue(
		TEXT("모바일 가짜 반사는 스킨 색을 덮지 않는 중립 유리 틴트"),
		Tuning.FakeReflectionTint.Equals(FLinearColor(0.72f, 0.78f, 0.86f, 1.0f)));
	TestTrue(
		TEXT("주 외벽은 색조를 유지하며 78%로 정리"),
		Tuning.PrimaryWallColorScale.Equals(FLinearColor(0.78f, 0.78f, 0.78f, 1.0f)));
	TestTrue(
		TEXT("창문은 스킨 고유 색을 보존하는 중립 RGB 스케일"),
		Tuning.WindowsColorScale.Equals(FLinearColor(0.70f, 0.70f, 0.70f, 1.0f)));
	TestTrue(
		TEXT("프레임은 중립 명도 정리"),
		Tuning.FramesColorScale.Equals(FLinearColor(0.60f, 0.60f, 0.62f, 1.0f)));

	TestTrue(
		TEXT("주 외벽 RGB 스케일은 알파를 보존"),
		ScaleRgbPreserveAlpha(
			FLinearColor(0.948f, 0.728f, 0.573f, 0.35f),
			Tuning.PrimaryWallColorScale)
			.Equals(FLinearColor(0.73944f, 0.56784f, 0.44694f, 0.35f), KINDA_SMALL_NUMBER));
	TestTrue(
		TEXT("창문 RGB 스케일은 알파를 보존"),
		ScaleRgbPreserveAlpha(
			FLinearColor(0.212f, 0.216f, 0.223f, 0.35f),
			Tuning.WindowsColorScale)
			.Equals(FLinearColor(0.1484f, 0.1512f, 0.1561f, 0.35f), KINDA_SMALL_NUMBER));
	TestTrue(
		TEXT("프레임 RGB 스케일은 알파를 보존"),
		ScaleRgbPreserveAlpha(
			FLinearColor(0.745f, 0.745f, 0.745f, 0.35f),
			Tuning.FramesColorScale)
			.Equals(FLinearColor(0.447f, 0.447f, 0.4619f, 0.35f), KINDA_SMALL_NUMBER));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
