#include "Misc/AutomationTest.h"
#include "Math/Box.h"
#include "Utils/MobileFacadeMaterialTuning.h"
#include "Entity/Building/BuildingBaseActor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/SoftObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRMobileFacadeQuietPlusContractTest,
	"CGR.Building.MobileFacade.QuietPlusContract",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRMobileFacadeQuietPlusContractTest::RunTest(const FString& Parameters)
{
	using namespace MobileFacadeMaterialTuning;
	const FQuietPlusTuning& Tuning = GetQuietPlusTuning();

	TestEqual(TEXT("스킨 원본 노멀 강도에 적용하는 배율"), Tuning.NormalIntensityScale, 0.28f);
	// 소비 노드가 없는 dead knob — 실제 점등 비율은 Window_Light_Density 가 정한다.
	TestEqual(TEXT("미사용 임계값 파라미터 계약"), Tuning.WindowLightThreshold, 0.85f);
	TestEqual(TEXT("창문 채도 배율"), Tuning.WindowsColorSaturation, 1.35f);
	TestTrue(
		TEXT("주 외벽은 색조를 유지하며 92%로 정리"),
		Tuning.PrimaryWallColorScale.Equals(FLinearColor(0.92f, 0.92f, 0.92f, 1.0f)));
	TestTrue(
		TEXT("창문 명도 스케일"),
		Tuning.WindowsColorScale.Equals(FLinearColor(0.62f, 0.62f, 0.62f, 1.0f)));
	TestTrue(
		TEXT("프레임은 중립 명도 정리"),
		Tuning.FramesColorScale.Equals(FLinearColor(0.60f, 0.60f, 0.60f, 1.0f)));

	TestTrue(
		TEXT("주 외벽 RGB 스케일은 알파를 보존"),
		ScaleRgbPreserveAlpha(
			FLinearColor(0.948f, 0.728f, 0.573f, 0.35f),
			Tuning.PrimaryWallColorScale)
			.Equals(FLinearColor(0.87216f, 0.66976f, 0.52716f, 0.35f), KINDA_SMALL_NUMBER));
	const FLinearColor WarmWindow(0.48f, 0.24f, 0.12f, 0.35f);
	const FLinearColor CoolWindow(0.12f, 0.31f, 0.53f, 0.35f);
	const FLinearColor WarmAdjusted = DeepenGlassColor(
		WarmWindow, Tuning.WindowsColorScale, Tuning.WindowsColorSaturation);
	const FLinearColor CoolAdjusted = DeepenGlassColor(
		CoolWindow, Tuning.WindowsColorScale, Tuning.WindowsColorSaturation);
	TestTrue(TEXT("웜톤 유리는 원본의 따뜻한 색조를 유지"), WarmAdjusted.R > WarmAdjusted.G && WarmAdjusted.G > WarmAdjusted.B);
	TestTrue(TEXT("쿨톤 유리는 원본의 차가운 색조를 유지"), CoolAdjusted.B > CoolAdjusted.G && CoolAdjusted.G > CoolAdjusted.R);
	TestEqual(TEXT("유리 알파 보존"), WarmAdjusted.A, WarmWindow.A);
	TestTrue(
		TEXT("프레임 RGB 스케일은 알파를 보존"),
		ScaleRgbPreserveAlpha(
			FLinearColor(0.745f, 0.745f, 0.745f, 0.35f),
			Tuning.FramesColorScale)
			.Equals(FLinearColor(0.447f, 0.447f, 0.447f, 0.35f), KINDA_SMALL_NUMBER));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRMobileFacadeBakedAOValidBoundsContractTest,
	"CGR.Building.MobileFacade.BakedAO.ValidBounds",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRMobileFacadeBakedAOValidBoundsContractTest::RunTest(const FString& Parameters)
{
	using namespace MobileFacadeMaterialTuning;
	const FBox ActorBounds(
		FVector(-100.0f, -100.0f, 100.0f),
		FVector(100.0f, 100.0f, 300.0f));
	FMobileBakedAOParameters ParametersOut;

	TestTrue(
		TEXT("유효한 액터 바운드는 BAO 파라미터를 산출"),
		TryCalculateBakedAOParameters(ActorBounds, ParametersOut));
	TestTrue(
		TEXT("BAO GroundZ는 액터 바운드의 MinZ"),
		FMath::IsNearlyEqual(ParametersOut.GroundZ, 100.0f, KINDA_SMALL_NUMBER));
	TestTrue(
		TEXT("BAO InvHeight는 1 / (MaxZ - MinZ)"),
		FMath::IsNearlyEqual(ParametersOut.InvHeight, 0.005f, KINDA_SMALL_NUMBER));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRMobileFacadeBakedAOInvalidBoundsContractTest,
	"CGR.Building.MobileFacade.BakedAO.InvalidBoundsFailClosed",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRMobileFacadeBakedAOInvalidBoundsContractTest::RunTest(const FString& Parameters)
{
	using namespace MobileFacadeMaterialTuning;
	const FBox InvalidBounds(ForceInit);
	FMobileBakedAOParameters ParametersOut;

	TestFalse(
		TEXT("무효한 액터 바운드는 BAO를 활성화하지 않음"),
		TryCalculateBakedAOParameters(InvalidBounds, ParametersOut));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRMobileFacadeBakedAOZeroHeightContractTest,
	"CGR.Building.MobileFacade.BakedAO.ZeroHeightFailClosed",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRMobileFacadeBakedAOZeroHeightContractTest::RunTest(const FString& Parameters)
{
	using namespace MobileFacadeMaterialTuning;
	const FBox ZeroHeightBounds(
		FVector(-100.0f, -100.0f, 100.0f),
		FVector(100.0f, 100.0f, 100.0f));
	FMobileBakedAOParameters ParametersOut;

	TestFalse(
		TEXT("높이가 0인 액터 바운드는 BAO를 활성화하지 않음"),
		TryCalculateBakedAOParameters(ZeroHeightBounds, ParametersOut));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRMobileFacadeWindowEmissiveMobileContractTest,
	"CGR.Building.MobileFacade.WindowEmissive.MobileES31Multiplier",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRMobileFacadeWindowEmissiveMobileContractTest::RunTest(const FString& Parameters)
{
	using namespace MobileFacadeMaterialTuning;
	const FLinearColor Source(0.25f, 0.50f, 0.75f, 0.35f);

	TestTrue(
		TEXT("휘도 상한 아래의 모바일 ES3_1 색은 RGB 4배와 알파를 보존"),
		ResolveWindowEmissiveColor(Source, true)
			.Equals(FLinearColor(1.0f, 2.0f, 3.0f, 0.35f), KINDA_SMALL_NUMBER));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRMobileFacadeWindowEmissiveHighlightLimitTest,
	"CGR.Building.MobileFacade.WindowEmissive.HighlightLimitPreservesHue",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRMobileFacadeWindowEmissiveHighlightLimitTest::RunTest(const FString& Parameters)
{
	using namespace MobileFacadeMaterialTuning;
	const FLinearColor Source(3.0f, 1.0f, 0.5f, 0.35f);
	const FLinearColor Adjusted = ResolveWindowEmissiveColor(Source, true);

	TestTrue(TEXT("강한 모바일 창문은 휘도 상한 안에 유지"),
		FMath::IsNearlyEqual(Adjusted.GetLuminance(), 2.4f, KINDA_SMALL_NUMBER));
	TestTrue(TEXT("채널별 클램프 없이 원본 RGB 비율 유지"),
		FMath::IsNearlyEqual(Adjusted.R * Source.G, Adjusted.G * Source.R, KINDA_SMALL_NUMBER)
			&& FMath::IsNearlyEqual(Adjusted.G * Source.B, Adjusted.B * Source.G, KINDA_SMALL_NUMBER));
	TestEqual(TEXT("발광 상한은 알파를 바꾸지 않음"), Adjusted.A, Source.A);
	TestTrue(TEXT("강한 원본도 데스크톱에서는 그대로 유지"),
		ResolveWindowEmissiveColor(Source, false).Equals(Source));
	TestTrue(TEXT("소등 색은 나눗셈 오류 없이 소등 유지"),
		ResolveWindowEmissiveColor(FLinearColor::Transparent, true).Equals(FLinearColor::Transparent));
	TestTrue(TEXT("원본 캐시를 재적용해도 발광 상한이 누적되지 않음"),
		ResolveWindowEmissiveColor(Source, true).Equals(Adjusted));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRMobileFacadeWindowEmissiveDesktopContractTest,
	"CGR.Building.MobileFacade.WindowEmissive.DesktopUnchanged",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRMobileFacadeWindowEmissiveDesktopContractTest::RunTest(const FString& Parameters)
{
	using namespace MobileFacadeMaterialTuning;
	const FLinearColor Source(0.25f, 0.50f, 0.75f, 0.35f);

	TestTrue(
		TEXT("데스크톱은 원본 emissive 색을 그대로 사용"),
		ResolveWindowEmissiveColor(Source, false)
			.Equals(FLinearColor(0.25f, 0.50f, 0.75f, 0.35f), KINDA_SMALL_NUMBER));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRBuildingWindowLightDensityContractTest,
	"CGR.Building.WindowLight.DensityPairContract",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRBuildingWindowLightDensityContractTest::RunTest(const FString& Parameters)
{
	// 점등 비율은 머티리얼이 액터 위치로 ±0.05 를 더하므로 평시 10~20%, 진행중 25~35% 가 된다.
	const ABuildingBaseActor* Defaults = GetDefault<ABuildingBaseActor>();

	TestEqual(TEXT("프로젝트 없는 평시 점등 비율"), Defaults->GetIdleWindowLightDensity(), 0.15f);
	TestEqual(TEXT("프로젝트 진행중 점등 비율"), Defaults->GetActiveWindowLightDensity(), 0.30f);
	TestTrue(
		TEXT("진행중이 평시보다 밝고 두 값 모두 0~1 분율"),
		Defaults->GetIdleWindowLightDensity() > 0.0f
			&& Defaults->GetIdleWindowLightDensity() < Defaults->GetActiveWindowLightDensity()
			&& Defaults->GetActiveWindowLightDensity() <= 1.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRMobileFacadeWindowEmissiveCachedSourceContractTest,
	"CGR.Building.MobileFacade.WindowEmissive.CachedSourceDoesNotAccumulate",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRMobileFacadeWindowEmissiveCachedSourceContractTest::RunTest(const FString& Parameters)
{
	using namespace MobileFacadeMaterialTuning;
	const FLinearColor CachedUnscaledSource(0.25f, 0.50f, 0.75f, 0.35f);
	const FLinearColor FirstMIDWrite = ResolveWindowEmissiveColor(CachedUnscaledSource, true);
	const FLinearColor ReappliedMIDWrite = ResolveWindowEmissiveColor(CachedUnscaledSource, true);

	TestTrue(
		TEXT("캐시한 scaling 전 원본을 재적용해도 RGB가 16배로 누적되지 않음"),
		FirstMIDWrite.Equals(FLinearColor(1.0f, 2.0f, 3.0f, 0.35f), KINDA_SMALL_NUMBER)
			&& ReappliedMIDWrite.Equals(FLinearColor(1.0f, 2.0f, 3.0f, 0.35f), KINDA_SMALL_NUMBER));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRMobileFacadePalettePreservationTest,
	"CGR.Building.MobileFacade.PalettePreservesSkinGlassAndReflection",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRMobileFacadePalettePreservationTest::RunTest(const FString& Parameters)
{
	using namespace MobileFacadeMaterialTuning;
	const TSoftObjectPtr<UMaterialInterface> FacadeMaster(
		FSoftObjectPath(TEXT("/Game/ModernBuildings/Materials/M_MB_BuildingMaterial_Complex.M_MB_BuildingMaterial_Complex")));
	UMaterialInterface* Master = FacadeMaster.LoadSynchronous();
	if (!TestNotNull(TEXT("production facade master"), Master)) { return false; }

	struct FPaletteCase
	{
		FLinearColor Window;
		FLinearColor Reflection;
		float WindowRoughness;
		float FrameRoughness;
		float ReflectionStrength;
		float NormalIntensity;
	};
	const FPaletteCase Palettes[] = {
		{FLinearColor(0.48f, 0.24f, 0.12f, 0.7f), FLinearColor(0.92f, 0.55f, 0.30f), 0.37f, 0.63f, 0.31f, 0.0f},
		{FLinearColor(0.12f, 0.31f, 0.53f, 0.7f), FLinearColor(0.30f, 0.62f, 0.94f), 0.58f, 0.42f, 0.64f, 0.8f}
	};
	const FName ScalarNames[] = {
		TEXT("Windows_Metallic"), TEXT("Windows_Roughness"),
		TEXT("Frames_Metallic"), TEXT("Frames_Roughness"),
		TEXT("Specular"), TEXT("Mobile_FakeReflection_Strength")
	};
	UMaterialInstanceDynamic* FacadeMID = UMaterialInstanceDynamic::Create(Master, GetTransientPackage());
	if (!TestNotNull(TEXT("transient facade MID"), FacadeMID)) { return false; }
	const int32 PaletteOrder[] = {0, 1, 0};
	for (const int32 PaletteIndex : PaletteOrder)
	{
		const FPaletteCase& Palette = Palettes[PaletteIndex];
		UMaterialInstanceDynamic* PaletteSource = UMaterialInstanceDynamic::Create(Master, GetTransientPackage());
		if (!TestNotNull(TEXT("transient palette source"), PaletteSource)) { return false; }

		PaletteSource->SetVectorParameterValue(TEXT("Windows_Color"), Palette.Window);
		PaletteSource->SetVectorParameterValue(TEXT("Mobile_FakeReflection_Tint"), Palette.Reflection);
		PaletteSource->SetScalarParameterValue(TEXT("Normal_Intensity"), Palette.NormalIntensity);
		const float Scalars[] = {0.07f + PaletteIndex * 0.21f, Palette.WindowRoughness,
			0.16f + PaletteIndex * 0.27f, Palette.FrameRoughness,
			0.29f + PaletteIndex * 0.36f, Palette.ReflectionStrength};
		for (int32 ParameterIndex = 0; ParameterIndex < UE_ARRAY_COUNT(ScalarNames); ++ParameterIndex)
		{
			PaletteSource->SetScalarParameterValue(ScalarNames[ParameterIndex], Scalars[ParameterIndex]);
		}

		FLinearColor FirstAppliedWindow;
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
			if (!TestTrue(TEXT("real facade application succeeds"),
				ApplyToDynamicMaterial(FacadeMID, PaletteSource))) { return false; }
			float ActualNormal = -1.0f;
			TestTrue(TEXT("normal detail follows each skin, including deliberately flat skins"),
				FacadeMID->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("Normal_Intensity")), ActualNormal)
					&& FMath::IsNearlyEqual(ActualNormal,
						Palette.NormalIntensity * GetQuietPlusTuning().NormalIntensityScale, KINDA_SMALL_NUMBER));
			for (int32 ParameterIndex = 0; ParameterIndex < UE_ARRAY_COUNT(ScalarNames); ++ParameterIndex)
			{
				float Actual = -1.0f;
				TestTrue(*FString::Printf(TEXT("palette %d pass %d keeps %s"), PaletteIndex, Pass,
					*ScalarNames[ParameterIndex].ToString()),
					FacadeMID->GetScalarParameterValue(FHashedMaterialParameterInfo(ScalarNames[ParameterIndex]), Actual)
						&& FMath::IsNearlyEqual(Actual, Scalars[ParameterIndex], KINDA_SMALL_NUMBER));
			}
			FLinearColor ActualWindow;
			FLinearColor ActualReflection;
			TestTrue(TEXT("reflection tint follows the palette"),
				FacadeMID->GetVectorParameterValue(
					FHashedMaterialParameterInfo(TEXT("Mobile_FakeReflection_Tint")), ActualReflection)
					&& ActualReflection.Equals(Palette.Reflection, KINDA_SMALL_NUMBER));
			TestTrue(TEXT("window alpha and warm/cool hue follow the palette"),
				FacadeMID->GetVectorParameterValue(FHashedMaterialParameterInfo(TEXT("Windows_Color")), ActualWindow)
					&& FMath::IsNearlyEqual(ActualWindow.A, Palette.Window.A, KINDA_SMALL_NUMBER)
					&& (PaletteIndex == 0 ? ActualWindow.R > ActualWindow.B : ActualWindow.B > ActualWindow.R));
			if (Pass == 0)
			{
				FirstAppliedWindow = ActualWindow;
			}
			else
			{
				TestTrue(TEXT("reapplying from palette does not accumulate window darkening"),
					ActualWindow.Equals(FirstAppliedWindow, KINDA_SMALL_NUMBER));
			}
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
