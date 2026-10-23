#include "Components/ReflectionCaptureComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/Engine.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SphereReflectionCapture.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureCube.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "TimeCycle/TimeCycleManager.h"
#include "TimeCycle/TimeCycleSky.h"
#include "TimeCycle/TimeCycleSkyLighting.h"
#include "TimeCycle/TimeCycleSkyVisualProfile.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr float ProfileTolerance = 0.0001f;
	const FName L2ColorGradingLutPropertyName(TEXT("L2ColorGradingLUT"));
	const FName DCL2ColorGradingLutPropertyName(TEXT("DCL2ColorGradingLUT"));

	class FScopedTimeCycleTestWorld
	{
	public:
		explicit FScopedTimeCycleTestWorld(ERHIFeatureLevel::Type InFeatureLevel)
		{
			TestWorld = UWorld::CreateWorld(
				EWorldType::Game,
				false,
				MakeUniqueObjectName(
					GetTransientPackage(),
					UWorld::StaticClass(),
					TEXT("TimeCycleVisualProfileTestWorld")),
				GetTransientPackage(),
				true,
				InFeatureLevel);
			if (TestWorld && GEngine)
			{
				FWorldContext& TestWorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
				TestWorldContext.SetCurrentWorld(TestWorld);
			}
		}

		~FScopedTimeCycleTestWorld()
		{
			if (!TestWorld)
			{
				return;
			}

			if (GEngine)
			{
				GEngine->DestroyWorldContext(TestWorld);
			}
			TestWorld->DestroyWorld(false);
		}

		UWorld* Get() const
		{
			return TestWorld;
		}

	private:
		UWorld* TestWorld = nullptr;
	};

	struct FExpectedVisualProfile
	{
		const TCHAR* Label;
		float Hour;
		EMainMapVisualPhase Phase;
		EMainMapLutRole LutRole;
		float LutIntensity;
		FLinearColor Saturation;
		FLinearColor Contrast;
		FLinearColor Gain;
		FLinearColor MidtoneSaturation;
		FLinearColor MidtoneGain;
	};

	void TestColor(
		FAutomationTestBase& AutomationTest,
		const FString& Label,
		const FLinearColor& Actual,
		const FLinearColor& Expected)
	{
		AutomationTest.TestTrue(Label, Actual.Equals(Expected, ProfileTolerance));
	}

	void TestVector4(
		FAutomationTestBase& AutomationTest,
		const FString& Label,
		const FVector4& Actual,
		const FVector4& Expected)
	{
		AutomationTest.TestTrue(Label, Actual.Equals(Expected, ProfileTolerance));
	}

	FObjectProperty* FindSerializedTextureProperty(
		FAutomationTestBase& AutomationTest,
		const FName PropertyName)
	{
		FProperty* ReflectedProperty =
			ATimeCycleSky::StaticClass()->FindPropertyByName(PropertyName);
		const FString Prefix = PropertyName.ToString();
		if (!AutomationTest.TestNotNull(Prefix + TEXT(" UPROPERTY exists"), ReflectedProperty))
		{
			return nullptr;
		}

		FObjectProperty* TextureProperty = CastField<FObjectProperty>(ReflectedProperty);
		if (!AutomationTest.TestNotNull(Prefix + TEXT(" is a hard object reference"), TextureProperty))
		{
			return nullptr;
		}

		AutomationTest.TestTrue(
			Prefix + TEXT(" references UTexture"),
			TextureProperty->PropertyClass->IsChildOf(UTexture::StaticClass()));
		AutomationTest.TestTrue(
			Prefix + TEXT(" is a TObjectPtr property"),
			ReflectedProperty->HasAnyPropertyFlags(CPF_TObjectPtr));
		AutomationTest.TestTrue(
			Prefix + TEXT(" is editable on the placed actor"),
			ReflectedProperty->HasAnyPropertyFlags(CPF_Edit)
				&& !ReflectedProperty->HasAnyPropertyFlags(CPF_DisableEditOnInstance | CPF_EditConst));
		AutomationTest.TestFalse(
			Prefix + TEXT(" is cooker-visible and serialized"),
			ReflectedProperty->HasAnyPropertyFlags(
				CPF_Transient
				| CPF_DuplicateTransient
				| CPF_NonPIEDuplicateTransient
				| CPF_EditorOnly
				| CPF_SkipSerialization));
		return TextureProperty;
	}

	bool SetSerializedTextureReference(
		FAutomationTestBase& AutomationTest,
		ATimeCycleSky& TimeCycleSky,
		const FName PropertyName,
		UTexture* Texture)
	{
		FObjectProperty* TextureProperty =
			FindSerializedTextureProperty(AutomationTest, PropertyName);
		if (!TextureProperty)
		{
			return false;
		}

		TextureProperty->SetObjectPropertyValue_InContainer(&TimeCycleSky, Texture);
		return true;
	}

	// 황혼 노브는 protected 라 리플렉션으로 넣는다. 이름은 unity 블롭 안에서 유일해야 한다.
	bool SetTimeCycleSkyFloatProperty(
		FAutomationTestBase& AutomationTest,
		ATimeCycleSky& TimeCycleSky,
		const TCHAR* PropertyName,
		float Value)
	{
		const FFloatProperty* Property = FindFProperty<FFloatProperty>(
			ATimeCycleSky::StaticClass(),
			PropertyName);
		if (!AutomationTest.TestNotNull(PropertyName, Property))
		{
			return false;
		}
		Property->SetPropertyValue_InContainer(&TimeCycleSky, Value);
		return true;
	}

	bool SetTimeCycleSkyBoolProperty(
		FAutomationTestBase& AutomationTest,
		ATimeCycleSky& TimeCycleSky,
		const TCHAR* PropertyName,
		bool bValue)
	{
		const FBoolProperty* Property = FindFProperty<FBoolProperty>(
			ATimeCycleSky::StaticClass(),
			PropertyName);
		if (!AutomationTest.TestNotNull(PropertyName, Property))
		{
			return false;
		}
		Property->SetPropertyValue_InContainer(&TimeCycleSky, bValue);
		return true;
	}

	bool SetSerializedLutReferences(
		FAutomationTestBase& AutomationTest,
		ATimeCycleSky& TimeCycleSky,
		UTexture* L2Texture,
		UTexture* DCL2Texture)
	{
		const bool bSetL2 = SetSerializedTextureReference(
			AutomationTest,
			TimeCycleSky,
			L2ColorGradingLutPropertyName,
			L2Texture);
		const bool bSetDCL2 = SetSerializedTextureReference(
			AutomationTest,
			TimeCycleSky,
			DCL2ColorGradingLutPropertyName,
			DCL2Texture);
		return bSetL2 && bSetDCL2;
	}

	UReflectionCaptureComponent* SpawnProductionE6Capture(
		FAutomationTestBase& AutomationTest,
		UWorld& TestWorld,
		const FName ActorName)
	{
		FActorSpawnParameters CaptureSpawnParameters;
		CaptureSpawnParameters.Name = ActorName;
		ASphereReflectionCapture* E6Capture =
			TestWorld.SpawnActor<ASphereReflectionCapture>(CaptureSpawnParameters);
		if (!AutomationTest.TestNotNull(TEXT("E6 sphere reflection capture"), E6Capture))
		{
			return nullptr;
		}

		const TSoftObjectPtr<UTextureCube> E6CubemapPath(FSoftObjectPath(
			TEXT("/Game/CompanyGrowth/Resources/Materials/MainMap/TC_StudioReflect.TC_StudioReflect")));
		UTextureCube* E6Cubemap = E6CubemapPath.LoadSynchronous();
		UReflectionCaptureComponent* E6Component = E6Capture->GetCaptureComponent();
		if (!AutomationTest.TestNotNull(TEXT("production TC_StudioReflect cubemap"), E6Cubemap)
			|| !AutomationTest.TestNotNull(TEXT("E6 reflection capture component"), E6Component))
		{
			return nullptr;
		}

		E6Component->ReflectionSourceType = EReflectionSourceType::SpecifiedCubemap;
		E6Component->Cubemap = E6Cubemap;
		E6Component->Brightness = 0.85f;
		E6Component->SetVisibility(true, true);
		return E6Component;
	}

	void TestDayProfileApplied(
		FAutomationTestBase& AutomationTest,
		const FPostProcessSettings& Settings,
		const UTexture* DCL2Texture)
	{
		AutomationTest.TestTrue(TEXT("Day overrides ColorGradingLUT"), Settings.bOverride_ColorGradingLUT);
		AutomationTest.TestTrue(TEXT("Day selects the serialized DCL2 LUT"), Settings.ColorGradingLUT == DCL2Texture);
		AutomationTest.TestTrue(TEXT("Day overrides ColorGradingIntensity"), Settings.bOverride_ColorGradingIntensity);
		AutomationTest.TestEqual(
			TEXT("Day LUT intensity"),
			Settings.ColorGradingIntensity,
			1.0f,
			ProfileTolerance);

		AutomationTest.TestTrue(TEXT("Day overrides global saturation"), Settings.bOverride_ColorSaturation);
		TestVector4(
			AutomationTest,
			TEXT("Day global saturation"),
			Settings.ColorSaturation,
			FVector4(1.0, 1.0, 1.0, 1.0));
		AutomationTest.TestTrue(TEXT("Day overrides global contrast"), Settings.bOverride_ColorContrast);
		TestVector4(
			AutomationTest,
			TEXT("Day global contrast"),
			Settings.ColorContrast,
			FVector4(1.0, 1.0, 1.0, 1.0));
		AutomationTest.TestTrue(TEXT("Day overrides global gain"), Settings.bOverride_ColorGain);
		TestVector4(
			AutomationTest,
			TEXT("Day global gain"),
			Settings.ColorGain,
			FVector4(1.0, 1.0, 1.0, 1.0));

		AutomationTest.TestTrue(
			TEXT("Day overrides midtone saturation"),
			Settings.bOverride_ColorSaturationMidtones);
		TestVector4(
			AutomationTest,
			TEXT("Day midtone saturation"),
			Settings.ColorSaturationMidtones,
			FVector4(1.15, 1.15, 1.15, 1.0));
		AutomationTest.TestTrue(TEXT("Day overrides midtone gain"), Settings.bOverride_ColorGainMidtones);
		TestVector4(
			AutomationTest,
			TEXT("Day midtone gain"),
			Settings.ColorGainMidtones,
			FVector4(1.0, 1.0, 1.0, 1.0));

		AutomationTest.TestTrue(TEXT("Profile overrides Sharpen"), Settings.bOverride_Sharpen);
		AutomationTest.TestEqual(TEXT("Profile Sharpen"), Settings.Sharpen, 0.4f, ProfileTolerance);
		AutomationTest.TestFalse(TEXT("Sharpen does not override FilmSlope"), Settings.bOverride_FilmSlope);
		AutomationTest.TestEqual(
			TEXT("Sharpen preserves FilmSlope"),
			Settings.FilmSlope,
			0.777f,
			ProfileTolerance);

		AutomationTest.TestTrue(TEXT("Profile overrides bloom intensity"), Settings.bOverride_BloomIntensity);
		AutomationTest.TestEqual(
			TEXT("Profile bloom intensity"),
			Settings.BloomIntensity,
			0.3f,
			ProfileTolerance);
		AutomationTest.TestTrue(TEXT("Profile overrides bloom threshold"), Settings.bOverride_BloomThreshold);
		AutomationTest.TestEqual(
			TEXT("Profile bloom threshold"),
			Settings.BloomThreshold,
			3.0f,
			ProfileTolerance);

		AutomationTest.TestTrue(
			TEXT("Profile overrides shadows boundary"),
			Settings.bOverride_ColorCorrectionShadowsMax);
		AutomationTest.TestEqual(
			TEXT("Profile shadows boundary"),
			Settings.ColorCorrectionShadowsMax,
			0.09f,
			ProfileTolerance);
		AutomationTest.TestTrue(
			TEXT("Profile overrides highlights boundary"),
			Settings.bOverride_ColorCorrectionHighlightsMin);
		AutomationTest.TestEqual(
			TEXT("Profile highlights boundary"),
			Settings.ColorCorrectionHighlightsMin,
			0.50f,
			ProfileTolerance);
	}

	void TestExpectedProfile(
		FAutomationTestBase& AutomationTest,
		const FExpectedVisualProfile& Expected)
	{
		const FTimeCycleSkyVisualProfile Actual =
			TimeCycleSkyVisualProfile::Calculate(Expected.Hour, 7.0f, 19.0f, 2.0f, 2.0f);
		const FString Prefix = FString::Printf(TEXT("%s (%g시)"), Expected.Label, Expected.Hour);

		AutomationTest.TestEqual(Prefix + TEXT(" phase"), Actual.Phase, Expected.Phase);
		AutomationTest.TestEqual(Prefix + TEXT(" LUT role"), Actual.LutRole, Expected.LutRole);
		AutomationTest.TestEqual(
			Prefix + TEXT(" LUT intensity"),
			Actual.LutIntensity,
			Expected.LutIntensity,
			ProfileTolerance);
		TestColor(AutomationTest, Prefix + TEXT(" saturation"), Actual.Saturation, Expected.Saturation);
		TestColor(AutomationTest, Prefix + TEXT(" contrast"), Actual.Contrast, Expected.Contrast);
		TestColor(AutomationTest, Prefix + TEXT(" gain"), Actual.Gain, Expected.Gain);
		TestColor(
			AutomationTest,
			Prefix + TEXT(" midtone saturation"),
			Actual.MidtoneSaturation,
			Expected.MidtoneSaturation);
		TestColor(
			AutomationTest,
			Prefix + TEXT(" midtone gain"),
			Actual.MidtoneGain,
			Expected.MidtoneGain);

		AutomationTest.TestEqual(
			Prefix + TEXT(" sharpen"), Actual.Sharpen, 0.4f, ProfileTolerance);
		AutomationTest.TestEqual(
			Prefix + TEXT(" bloom intensity"), Actual.BloomIntensity, 0.3f, ProfileTolerance);
		AutomationTest.TestEqual(
			Prefix + TEXT(" bloom threshold"), Actual.BloomThreshold, 3.0f, ProfileTolerance);
		AutomationTest.TestEqual(
			Prefix + TEXT(" shadows max"), Actual.ShadowsMax, 0.09f, ProfileTolerance);
		AutomationTest.TestEqual(
			Prefix + TEXT(" highlights min"), Actual.HighlightsMin, 0.50f, ProfileTolerance);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRTimeCycleSkyVisualProfilePhaseBoundaryTest,
	"CGR.TimeCycleSky.VisualProfile.PhaseBoundaries",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRTimeCycleSkyVisualProfilePhaseBoundaryTest::RunTest(const FString& Parameters)
{
	struct FPhaseCase
	{
		float Hour;
		EMainMapVisualPhase ExpectedPhase;
	};

	const FPhaseCase Cases[] = {
		{0.0f, EMainMapVisualPhase::Night},
		{4.999f, EMainMapVisualPhase::Night},
		{5.0f, EMainMapVisualPhase::Dawn},
		{6.999f, EMainMapVisualPhase::Dawn},
		{7.0f, EMainMapVisualPhase::Day},
		{16.999f, EMainMapVisualPhase::Day},
		{17.0f, EMainMapVisualPhase::Dusk},
		{18.999f, EMainMapVisualPhase::Dusk},
		{19.0f, EMainMapVisualPhase::Night},
		{23.999f, EMainMapVisualPhase::Night},
	};

	for (const FPhaseCase& TestCase : Cases)
	{
		const FTimeCycleSkyVisualProfile Profile =
			TimeCycleSkyVisualProfile::Calculate(TestCase.Hour, 7.0f, 19.0f, 2.0f, 2.0f);
		TestEqual(
			*FString::Printf(TEXT("%g시 phase 경계"), TestCase.Hour),
			Profile.Phase,
			TestCase.ExpectedPhase);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRTimeCycleSkyVisualProfileConfigurableBlendBoundaryTest,
	"CGR.TimeCycleSky.VisualProfile.ConfigurableBlendBoundaries",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRTimeCycleSkyVisualProfileConfigurableBlendBoundaryTest::RunTest(const FString& Parameters)
{
	struct FConfigurableBoundaryCase
	{
		float Hour;
		EMainMapVisualPhase ExpectedPhase;
		EMainMapLutRole ExpectedLutRole;
	};

	const FConfigurableBoundaryCase Cases[] = {
		{5.999f, EMainMapVisualPhase::Night, EMainMapLutRole::L2},
		{6.0f, EMainMapVisualPhase::Dawn, EMainMapLutRole::L2},
		{6.999f, EMainMapVisualPhase::Dawn, EMainMapLutRole::L2},
		{7.0f, EMainMapVisualPhase::Day, EMainMapLutRole::DCL2},
		{15.999f, EMainMapVisualPhase::Day, EMainMapLutRole::DCL2},
		{16.0f, EMainMapVisualPhase::Dusk, EMainMapLutRole::DCL2},
		{18.999f, EMainMapVisualPhase::Dusk, EMainMapLutRole::L2},
		{19.0f, EMainMapVisualPhase::Night, EMainMapLutRole::L2},
	};

	for (const FConfigurableBoundaryCase& TestCase : Cases)
	{
		const FTimeCycleSkyVisualProfile Profile = TimeCycleSkyVisualProfile::Calculate(
			TestCase.Hour,
			7.0f,
			19.0f,
			1.0f,
			3.0f);
		const FString Prefix = FString::Printf(TEXT("%g시 configurable blend"), TestCase.Hour);
		TestEqual(Prefix + TEXT(" phase"), Profile.Phase, TestCase.ExpectedPhase);
		TestEqual(Prefix + TEXT(" LUT role"), Profile.LutRole, TestCase.ExpectedLutRole);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRTimeCycleSkyVisualProfileDuskLutBridgeRejectsHardSwitchTest,
	"CGR.TimeCycleSky.VisualProfile.DuskLutBridgeRejectsHardSwitch",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRTimeCycleSkyVisualProfileDuskLutBridgeRejectsHardSwitchTest::RunTest(
	const FString& Parameters)
{
	struct FDuskLutCase
	{
		float Hour;
		EMainMapVisualPhase ExpectedPhase;
		EMainMapLutRole ExpectedLutRole;
		float ExpectedLutIntensity;
	};

	// Mutation caught: switching from DCL2 directly to full-strength L2 at 17:00.
	const FDuskLutCase Cases[] = {
		{17.0f, EMainMapVisualPhase::Dusk, EMainMapLutRole::DCL2, 1.0f},
		{17.5f, EMainMapVisualPhase::Dusk, EMainMapLutRole::DCL2, 0.5f},
		{18.0f, EMainMapVisualPhase::Dusk, EMainMapLutRole::L2, 0.0f},
		{18.5f, EMainMapVisualPhase::Dusk, EMainMapLutRole::L2, 0.35f},
		{19.0f, EMainMapVisualPhase::Night, EMainMapLutRole::L2, 0.70f},
	};

	for (const FDuskLutCase& TestCase : Cases)
	{
		const FTimeCycleSkyVisualProfile Profile = TimeCycleSkyVisualProfile::Calculate(
			TestCase.Hour,
			7.0f,
			19.0f,
			2.0f,
			2.0f);
		const FString Prefix = FString::Printf(TEXT("%g시 황혼 LUT bridge"), TestCase.Hour);

		TestEqual(Prefix + TEXT(" phase"), Profile.Phase, TestCase.ExpectedPhase);
		TestEqual(Prefix + TEXT(" LUT role"), Profile.LutRole, TestCase.ExpectedLutRole);
		TestEqual(
			Prefix + TEXT(" LUT intensity"),
			Profile.LutIntensity,
			TestCase.ExpectedLutIntensity,
			ProfileTolerance);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRTimeCycleSkyVisualProfileDuskGradingRejectsFlatProfileAndBoundaryPopTest,
	"CGR.TimeCycleSky.VisualProfile.DuskGradingRejectsFlatProfileAndBoundaryPop",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRTimeCycleSkyVisualProfileDuskGradingRejectsFlatProfileAndBoundaryPopTest::RunTest(
	const FString& Parameters)
{
	struct FDuskGradingCase
	{
		float Hour;
		FLinearColor ExpectedSaturation;
		FLinearColor ExpectedContrast;
		FLinearColor ExpectedGain;
	};

	// Mutation caught: keeping one static Dusk grade and popping to Day/Night values at the boundaries.
	const FDuskGradingCase Cases[] = {
		{
			17.0f,
			FLinearColor(1.0f, 1.0f, 1.0f, 1.0f),
			FLinearColor(1.0f, 1.0f, 1.0f, 1.0f),
			FLinearColor(1.0f, 1.0f, 1.0f, 1.0f),
		},
		{
			17.5f,
			FLinearColor(1.02f, 1.02f, 1.02f, 1.0f),
			FLinearColor(1.015f, 1.015f, 1.015f, 1.0f),
			FLinearColor(1.02f, 1.0f, 0.97f, 1.0f),
		},
		{
			18.0f,
			FLinearColor(1.04f, 1.04f, 1.04f, 1.0f),
			FLinearColor(1.03f, 1.03f, 1.03f, 1.0f),
			FLinearColor(1.04f, 1.0f, 0.94f, 1.0f),
		},
		{
			18.5f,
			FLinearColor(1.07f, 1.07f, 1.07f, 1.0f),
			FLinearColor(1.025f, 1.025f, 1.025f, 1.0f),
			FLinearColor(1.04f, 1.02f, 0.99f, 1.0f),
		},
		{
			19.0f,
			FLinearColor(1.10f, 1.10f, 1.10f, 1.0f),
			FLinearColor(1.02f, 1.02f, 1.02f, 1.0f),
			FLinearColor(1.04f, 1.04f, 1.04f, 1.0f),
		},
	};

	for (const FDuskGradingCase& TestCase : Cases)
	{
		const FTimeCycleSkyVisualProfile Profile = TimeCycleSkyVisualProfile::Calculate(
			TestCase.Hour,
			7.0f,
			19.0f,
			2.0f,
			2.0f);
		const FString Prefix = FString::Printf(TEXT("%g시 황혼 grading"), TestCase.Hour);

		TestColor(*this, Prefix + TEXT(" saturation"), Profile.Saturation, TestCase.ExpectedSaturation);
		TestColor(*this, Prefix + TEXT(" contrast"), Profile.Contrast, TestCase.ExpectedContrast);
		TestColor(*this, Prefix + TEXT(" gain"), Profile.Gain, TestCase.ExpectedGain);
		TestColor(
			*this,
			Prefix + TEXT(" midtone saturation continuity"),
			Profile.MidtoneSaturation,
			FLinearColor(1.15f, 1.15f, 1.15f, 1.0f));
		TestColor(
			*this,
			Prefix + TEXT(" midtone gain"),
			Profile.MidtoneGain,
			FLinearColor::White);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRTimeCycleSkyVisualProfileRepresentativeLookTest,
	"CGR.TimeCycleSky.VisualProfile.RepresentativeLooks",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRTimeCycleSkyVisualProfileRepresentativeLookTest::RunTest(const FString& Parameters)
{
	const FExpectedVisualProfile Cases[] = {
		{
			TEXT("Dawn"),
			5.5f,
			EMainMapVisualPhase::Dawn,
			EMainMapLutRole::L2,
			0.80f,
			FLinearColor(1.03f, 1.03f, 1.03f, 1.0f),
			FLinearColor(1.02f, 1.02f, 1.02f, 1.0f),
			FLinearColor(1.025f, 1.0f, 0.96f, 1.0f),
			FLinearColor::White,
			FLinearColor(1.10f, 1.10f, 1.10f, 1.0f),
		},
		{
			TEXT("Day 09"),
			9.0f,
			EMainMapVisualPhase::Day,
			EMainMapLutRole::DCL2,
			1.0f,
			FLinearColor::White,
			FLinearColor::White,
			FLinearColor::White,
			FLinearColor(1.15f, 1.15f, 1.15f, 1.0f),
			FLinearColor::White,
		},
		{
			TEXT("Day 12"),
			12.0f,
			EMainMapVisualPhase::Day,
			EMainMapLutRole::DCL2,
			1.0f,
			FLinearColor::White,
			FLinearColor::White,
			FLinearColor::White,
			FLinearColor(1.15f, 1.15f, 1.15f, 1.0f),
			FLinearColor::White,
		},
		{
			TEXT("Day 16"),
			16.0f,
			EMainMapVisualPhase::Day,
			EMainMapLutRole::DCL2,
			1.0f,
			FLinearColor::White,
			FLinearColor::White,
			FLinearColor::White,
			FLinearColor(1.15f, 1.15f, 1.15f, 1.0f),
			FLinearColor::White,
		},
		{
			TEXT("Dusk"),
			18.0f,
			EMainMapVisualPhase::Dusk,
			EMainMapLutRole::L2,
			0.0f,
			FLinearColor(1.04f, 1.04f, 1.04f, 1.0f),
			FLinearColor(1.03f, 1.03f, 1.03f, 1.0f),
			FLinearColor(1.04f, 1.0f, 0.94f, 1.0f),
			FLinearColor(1.15f, 1.15f, 1.15f, 1.0f),
			FLinearColor::White,
		},
		{
			TEXT("Night 19:30"),
			19.5f,
			EMainMapVisualPhase::Night,
			EMainMapLutRole::L2,
			0.70f,
			FLinearColor(1.10f, 1.10f, 1.10f, 1.0f),
			FLinearColor(1.02f, 1.02f, 1.02f, 1.0f),
			FLinearColor(1.04f, 1.04f, 1.04f, 1.0f),
			FLinearColor(1.15f, 1.15f, 1.15f, 1.0f),
			FLinearColor::White,
		},
		{
			TEXT("Night 00"),
			0.0f,
			EMainMapVisualPhase::Night,
			EMainMapLutRole::L2,
			0.70f,
			FLinearColor(1.10f, 1.10f, 1.10f, 1.0f),
			FLinearColor(1.02f, 1.02f, 1.02f, 1.0f),
			FLinearColor(1.04f, 1.04f, 1.04f, 1.0f),
			FLinearColor(1.15f, 1.15f, 1.15f, 1.0f),
			FLinearColor::White,
		},
	};

	for (const FExpectedVisualProfile& Expected : Cases)
	{
		TestExpectedProfile(*this, Expected);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRTimeCycleSkyVisualProfileHourNormalizationTest,
	"CGR.TimeCycleSky.VisualProfile.HourNormalization",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRTimeCycleSkyVisualProfileHourNormalizationTest::RunTest(const FString& Parameters)
{
	struct FNormalizationCase
	{
		float InputHour;
		EMainMapVisualPhase ExpectedPhase;
		EMainMapLutRole ExpectedLutRole;
		float ExpectedLutIntensity;
	};

	const FNormalizationCase Cases[] = {
		{-18.5f, EMainMapVisualPhase::Dawn, EMainMapLutRole::L2, 0.80f},
		{29.5f, EMainMapVisualPhase::Dawn, EMainMapLutRole::L2, 0.80f},
		{-15.0f, EMainMapVisualPhase::Day, EMainMapLutRole::DCL2, 1.0f},
		{42.0f, EMainMapVisualPhase::Dusk, EMainMapLutRole::L2, 0.0f},
		{-4.5f, EMainMapVisualPhase::Night, EMainMapLutRole::L2, 0.70f},
		{48.0f, EMainMapVisualPhase::Night, EMainMapLutRole::L2, 0.70f},
	};

	for (const FNormalizationCase& TestCase : Cases)
	{
		const FTimeCycleSkyVisualProfile Profile =
			TimeCycleSkyVisualProfile::Calculate(TestCase.InputHour, 7.0f, 19.0f, 2.0f, 2.0f);
		const FString Prefix = FString::Printf(TEXT("%g시 정규화"), TestCase.InputHour);

		TestEqual(Prefix + TEXT(" phase"), Profile.Phase, TestCase.ExpectedPhase);
		TestEqual(Prefix + TEXT(" LUT role"), Profile.LutRole, TestCase.ExpectedLutRole);
		TestEqual(
			Prefix + TEXT(" LUT intensity"),
			Profile.LutIntensity,
			TestCase.ExpectedLutIntensity,
			ProfileTolerance);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRTimeCycleSkyProductionDefaultsTest,
	"CGR.TimeCycleSky.VisualProfile.ProductionDefaults",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRTimeCycleSkyProductionDefaultsTest::RunTest(const FString& Parameters)
{
	const ATimeCycleManager* DefaultManager = GetDefault<ATimeCycleManager>();
	if (!TestNotNull(TEXT("TimeCycleManager CDO"), DefaultManager))
	{
		return false;
	}

	const FTimeCycleCode SunSetTime = DefaultManager->GetSunSetTime();
	TestEqual(TEXT("production sunset hour"), SunSetTime.Hours, 19);
	TestEqual(TEXT("production sunset minute"), SunSetTime.Minutes, 0);
	TestEqual(TEXT("production sunset second"), SunSetTime.Seconds, 0);

	const ATimeCycleSky* DefaultSky = GetDefault<ATimeCycleSky>();
	if (!TestNotNull(TEXT("TimeCycleSky CDO"), DefaultSky))
	{
		return false;
	}

	const FTimeCycleSkyNightLook NightLook = DefaultSky->GetNightLook();
	TestEqual(
		TEXT("production general desktop night skylight intensity"),
		NightLook.SkyNightIntensity,
		0.20f,
		ProfileTolerance);
	TestEqual(
		TEXT("production general desktop night directional intensity"),
		NightLook.NightDirectionalIntensity,
		0.08f,
		ProfileTolerance);
	TestEqual(
		TEXT("production night directional temperature"),
		NightLook.NightDirectionalTemperature,
		9000.0f,
		ProfileTolerance);
	TestColor(
		*this,
		TEXT("production general desktop night ambient color"),
		NightLook.NightAmbientColor,
		FLinearColor(0.5149176654f, 0.4452011945f, 0.3762621230f, 1.0f));
	TestEqual(
		TEXT("production night exposure bias"),
		NightLook.NightExposureBias,
		0.20f,
		ProfileTolerance);

	const FFloatProperty* SunAzimuthProperty = FindFProperty<FFloatProperty>(
		ATimeCycleSky::StaticClass(),
		TEXT("SunAzimuth"));
	if (!TestNotNull(TEXT("SunAzimuth reflected property"), SunAzimuthProperty))
	{
		return false;
	}
	TestEqual(
		TEXT("production sun azimuth"),
		SunAzimuthProperty->GetPropertyValue_InContainer(DefaultSky),
		35.0f,
		ProfileTolerance);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRTimeCycleSkySerializedLutReferencesTest,
	"CGR.TimeCycleSky.VisualProfile.SerializedLutReferences",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRTimeCycleSkySerializedLutReferencesTest::RunTest(const FString& Parameters)
{
	FObjectProperty* L2Property = FindSerializedTextureProperty(
		*this,
		L2ColorGradingLutPropertyName);
	FObjectProperty* DCL2Property = FindSerializedTextureProperty(
		*this,
		DCL2ColorGradingLutPropertyName);
	const ATimeCycleSky* DefaultSky = GetDefault<ATimeCycleSky>();
	if (!L2Property || !DCL2Property || !TestNotNull(TEXT("TimeCycleSky CDO"), DefaultSky))
	{
		return false;
	}

	TestNull(
		TEXT("L2 LUT remains unassigned until the placed actor is authored"),
		L2Property->GetObjectPropertyValue_InContainer(DefaultSky));
	TestNull(
		TEXT("DCL2 LUT remains unassigned until the placed actor is authored"),
		DCL2Property->GetObjectPropertyValue_InContainer(DefaultSky));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRTimeCycleSkyES31ProfileApplicationLifecycleTest,
	"CGR.TimeCycleSky.VisualProfile.ES31ApplicationLifecycle",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRTimeCycleSkyES31ProfileApplicationLifecycleTest::RunTest(const FString& Parameters)
{
	FScopedTimeCycleTestWorld TestWorldScope(ERHIFeatureLevel::ES3_1);
	UWorld* TestWorld = TestWorldScope.Get();
	if (!TestNotNull(TEXT("ES3_1 game test world"), TestWorld))
	{
		return false;
	}

	ATimeCycleManager* Manager = TestWorld->SpawnActor<ATimeCycleManager>();
	ATimeCycleSky* TimeCycleSky = TestWorld->SpawnActor<ATimeCycleSky>();
	if (!TestNotNull(TEXT("ES3_1 TimeCycleManager"), Manager)
		|| !TestNotNull(TEXT("ES3_1 TimeCycleSky"), TimeCycleSky))
	{
		return false;
	}

	UTexture2D* L2Texture = NewObject<UTexture2D>(TimeCycleSky, TEXT("TestL2ColorGradingLUT"));
	UTexture2D* DCL2Texture = NewObject<UTexture2D>(TimeCycleSky, TEXT("TestDCL2ColorGradingLUT"));
	if (!TestNotNull(TEXT("test L2 texture"), L2Texture)
		|| !TestNotNull(TEXT("test DCL2 texture"), DCL2Texture)
		|| !SetSerializedLutReferences(*this, *TimeCycleSky, L2Texture, DCL2Texture))
	{
		return false;
	}

	FTimeCycleCode DayTime;
	DayTime.Hours = 9;
	Manager->SetCurrentTime(DayTime);

	// Mutation caught: a one-shot PPV search permanently misses a volume that appears after the first update.
	TimeCycleSky->Tick(0.11f);
	APostProcessVolume* LatePostProcessVolume = TestWorld->SpawnActor<APostProcessVolume>();
	if (!TestNotNull(TEXT("late unbound post-process volume"), LatePostProcessVolume))
	{
		return false;
	}
	LatePostProcessVolume->bUnbound = true;
	LatePostProcessVolume->Settings.FilmSlope = 0.777f;
	LatePostProcessVolume->Settings.bOverride_FilmSlope = false;

	TimeCycleSky->Tick(0.11f);
	TestDayProfileApplied(*this, LatePostProcessVolume->Settings, DCL2Texture);

	// Mutation caught: writing the unchanged phase every 10 Hz rebuilds the combined LUT continuously.
	LatePostProcessVolume->Settings.ColorGradingIntensity = 0.314f;
	TimeCycleSky->Tick(0.11f);
	TestEqual(
		TEXT("unchanged Day phase does not rewrite profile"),
		LatePostProcessVolume->Settings.ColorGradingIntensity,
		0.314f,
		ProfileTolerance);

	// Mutation caught: expanding the Dusk-only hour cache invalidation to every visual phase.
	FTimeCycleCode LaterDayTime;
	LaterDayTime.Hours = 10;
	Manager->SetCurrentTime(LaterDayTime);
	TimeCycleSky->Tick(0.11f);
	TestEqual(
		TEXT("Day hour change preserves an external profile mutation"),
		LatePostProcessVolume->Settings.ColorGradingIntensity,
		0.314f,
		ProfileTolerance);

	FTimeCycleCode DuskTime;
	DuskTime.Hours = 18;
	Manager->SetCurrentTime(DuskTime);
	TimeCycleSky->Tick(0.11f);
	TestTrue(
		TEXT("Day to Dusk phase change selects L2"),
		LatePostProcessVolume->Settings.ColorGradingLUT == L2Texture);
	TestEqual(
		TEXT("Day to Dusk midpoint swaps LUT at zero intensity"),
		LatePostProcessVolume->Settings.ColorGradingIntensity,
		0.0f,
		ProfileTolerance);
	TestVector4(
		*this,
		TEXT("Day to Dusk phase change reapplies grading"),
		LatePostProcessVolume->Settings.ColorSaturation,
		FVector4(1.04, 1.04, 1.04, 1.0));

	// Mutation caught: caching only the phase prevents recovery when a serialized LUT becomes valid again.
	if (!SetSerializedTextureReference(
			*this,
			*TimeCycleSky,
			L2ColorGradingLutPropertyName,
			nullptr))
	{
		return false;
	}
	LatePostProcessVolume->Settings.ColorGradingLUT = nullptr;
	LatePostProcessVolume->Settings.ColorGradingIntensity = 0.271f;
	TimeCycleSky->Tick(0.11f);
	TestEqual(
		TEXT("invalid Dusk LUT does not write an incomplete profile"),
		LatePostProcessVolume->Settings.ColorGradingIntensity,
		0.271f,
		ProfileTolerance);

	if (!SetSerializedTextureReference(
			*this,
			*TimeCycleSky,
			L2ColorGradingLutPropertyName,
			L2Texture))
	{
		return false;
	}
	TimeCycleSky->Tick(0.11f);
	TestTrue(
		TEXT("restored Dusk LUT reapplies without a phase change"),
		LatePostProcessVolume->Settings.ColorGradingLUT == L2Texture);
	TestEqual(
		TEXT("restored Dusk LUT reapplies profile intensity"),
		LatePostProcessVolume->Settings.ColorGradingIntensity,
		0.0f,
		ProfileTolerance);

	FTimeCycleCode NightTime;
	NightTime.Hours = 0;
	Manager->SetCurrentTime(NightTime);
	TimeCycleSky->Tick(0.11f);
	TestEqual(
		TEXT("Dusk to Night phase change applies the night profile"),
		LatePostProcessVolume->Settings.ColorGradingIntensity,
		0.70f,
		ProfileTolerance);

	LatePostProcessVolume->Settings.ColorGradingIntensity = 0.619f;
	FTimeCycleCode LaterNightTime;
	LaterNightTime.Hours = 1;
	Manager->SetCurrentTime(LaterNightTime);
	TimeCycleSky->Tick(0.11f);
	TestEqual(
		TEXT("Night hour change preserves an external profile mutation"),
		LatePostProcessVolume->Settings.ColorGradingIntensity,
		0.619f,
		ProfileTolerance);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRTimeCycleSkyES31DuskSamePhaseUpdateRejectsPhaseOnlyCacheTest,
	"CGR.TimeCycleSky.VisualProfile.ES31DuskSamePhaseUpdateRejectsPhaseOnlyCache",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRTimeCycleSkyES31DuskSamePhaseUpdateRejectsPhaseOnlyCacheTest::RunTest(
	const FString& Parameters)
{
	FScopedTimeCycleTestWorld TestWorldScope(ERHIFeatureLevel::ES3_1);
	UWorld* TestWorld = TestWorldScope.Get();
	if (!TestNotNull(TEXT("ES3_1 dusk same-phase game test world"), TestWorld))
	{
		return false;
	}

	ATimeCycleManager* Manager = TestWorld->SpawnActor<ATimeCycleManager>();
	ATimeCycleSky* TimeCycleSky = TestWorld->SpawnActor<ATimeCycleSky>();
	APostProcessVolume* PostProcessVolume = TestWorld->SpawnActor<APostProcessVolume>();
	if (!TestNotNull(TEXT("ES3_1 dusk TimeCycleManager"), Manager)
		|| !TestNotNull(TEXT("ES3_1 dusk TimeCycleSky"), TimeCycleSky)
		|| !TestNotNull(TEXT("ES3_1 dusk unbound post-process volume"), PostProcessVolume))
	{
		return false;
	}

	UTexture2D* L2Texture = NewObject<UTexture2D>(TimeCycleSky, TEXT("DuskTestL2ColorGradingLUT"));
	UTexture2D* DCL2Texture = NewObject<UTexture2D>(TimeCycleSky, TEXT("DuskTestDCL2ColorGradingLUT"));
	if (!SetSerializedLutReferences(*this, *TimeCycleSky, L2Texture, DCL2Texture))
	{
		return false;
	}

	PostProcessVolume->bUnbound = true;

	FTimeCycleCode DuskQuarterTime;
	DuskQuarterTime.Hours = 17;
	DuskQuarterTime.Minutes = 15;
	Manager->SetCurrentTime(DuskQuarterTime);
	TimeCycleSky->Tick(0.11f);

	TestTrue(
		TEXT("17:15 selects DCL2 in the first dusk half"),
		PostProcessVolume->Settings.ColorGradingLUT == DCL2Texture);
	TestEqual(
		TEXT("17:15 starts DCL2 fade at 0.75"),
		PostProcessVolume->Settings.ColorGradingIntensity,
		0.75f,
		ProfileTolerance);
	TestVector4(
		*this,
		TEXT("17:15 applies the first-quarter saturation"),
		PostProcessVolume->Settings.ColorSaturation,
		FVector4(1.01, 1.01, 1.01, 1.0));

	FTimeCycleCode LaterSameHalfTime;
	LaterSameHalfTime.Hours = 17;
	LaterSameHalfTime.Minutes = 45;
	Manager->SetCurrentTime(LaterSameHalfTime);
	TimeCycleSky->Tick(0.11f);

	// Mutation caught: phase/LUT-only caching skips a visual update while both remain Dusk/DCL2.
	TestTrue(
		TEXT("17:45 remains on DCL2 in the first dusk half"),
		PostProcessVolume->Settings.ColorGradingLUT == DCL2Texture);
	TestEqual(
		TEXT("same Dusk and DCL2 phase still updates intensity to 0.25"),
		PostProcessVolume->Settings.ColorGradingIntensity,
		0.25f,
		ProfileTolerance);
	TestVector4(
		*this,
		TEXT("same Dusk and DCL2 phase still updates saturation to 1.03"),
		PostProcessVolume->Settings.ColorSaturation,
		FVector4(1.03, 1.03, 1.03, 1.0));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRTimeCycleSkyES31ReflectionCaptureLifecycleTest,
	"CGR.TimeCycleSky.VisualProfile.ES31ReflectionCaptureLifecycle",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRTimeCycleSkyES31ReflectionCaptureLifecycleTest::RunTest(const FString& Parameters)
{
	FScopedTimeCycleTestWorld TestWorldScope(ERHIFeatureLevel::ES3_1);
	UWorld* TestWorld = TestWorldScope.Get();
	if (!TestNotNull(TEXT("ES3_1 reflection lifecycle game world"), TestWorld))
	{
		return false;
	}

	ATimeCycleManager* Manager = TestWorld->SpawnActor<ATimeCycleManager>();
	ATimeCycleSky* TimeCycleSky = TestWorld->SpawnActor<ATimeCycleSky>();
	APostProcessVolume* PostProcessVolume = TestWorld->SpawnActor<APostProcessVolume>();
	if (!TestNotNull(TEXT("ES3_1 TimeCycleManager"), Manager)
		|| !TestNotNull(TEXT("ES3_1 TimeCycleSky"), TimeCycleSky)
		|| !TestNotNull(TEXT("ES3_1 unbound post-process volume"), PostProcessVolume))
	{
		return false;
	}

	UDirectionalLightComponent* SunLight =
		TimeCycleSky->FindComponentByClass<UDirectionalLightComponent>();
	USkyLightComponent* SkyLight = TimeCycleSky->FindComponentByClass<USkyLightComponent>();
	UReflectionCaptureComponent* E6Component = SpawnProductionE6Capture(
		*this,
		*TestWorld,
		TEXT("CGR_E6_StaticReflectionCapture"));
	if (!TestNotNull(TEXT("ES3_1 directional light"), SunLight)
		|| !TestNotNull(TEXT("ES3_1 sky light"), SkyLight)
		|| !E6Component)
	{
		return false;
	}

	PostProcessVolume->bUnbound = true;

	FTimeCycleCode DayTime;
	DayTime.Hours = 12;
	Manager->SetCurrentTime(DayTime);
	TimeCycleSky->Tick(0.11f);
	TestTrue(TEXT("ES3_1 E6 reflection remains visible at Day 12"), E6Component->IsVisible());

	FTimeCycleCode NightTime;
	NightTime.Hours = 0;
	Manager->SetCurrentTime(NightTime);
	TimeCycleSky->Tick(0.11f);
	TestFalse(TEXT("ES3_1 E6 reflection is hidden at Night 00"), E6Component->IsVisible());
	TestEqual(
		TEXT("ES3_1 Night 00 uses B2 directional intensity"),
		SunLight->Intensity,
		0.07f,
		ProfileTolerance);
	TestEqual(
		TEXT("ES3_1 Night 00 uses B2 directional temperature"),
		SunLight->Temperature,
		9000.0f,
		ProfileTolerance);
	TestEqual(
		TEXT("ES3_1 Night 00 uses B2 skylight intensity"),
		SkyLight->Intensity,
		0.17f,
		ProfileTolerance);
	TestColor(
		*this,
		TEXT("ES3_1 Night 00 uses B2 H3 ambient color"),
		SkyLight->GetLightColor(),
		FLinearColor(0.3762621284f, 0.4564110339f, 0.5775804520f, 1.0f));
	TestEqual(
		TEXT("ES3_1 Night 00 uses B2 exposure"),
		PostProcessVolume->Settings.AutoExposureBias,
		0.20f,
		ProfileTolerance);

	Manager->SetCurrentTime(DayTime);
	TimeCycleSky->Tick(0.11f);
	TestTrue(TEXT("ES3_1 E6 reflection is restored at Day 12"), E6Component->IsVisible());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRTimeCycleSkyDesktopProfilePreservationTest,
	"CGR.TimeCycleSky.VisualProfile.DesktopPreservation",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRTimeCycleSkyDesktopProfilePreservationTest::RunTest(const FString& Parameters)
{
	FScopedTimeCycleTestWorld TestWorldScope(ERHIFeatureLevel::SM5);
	UWorld* TestWorld = TestWorldScope.Get();
	if (!TestNotNull(TEXT("desktop game test world"), TestWorld))
	{
		return false;
	}

	ATimeCycleManager* Manager = TestWorld->SpawnActor<ATimeCycleManager>();
	ATimeCycleSky* TimeCycleSky = TestWorld->SpawnActor<ATimeCycleSky>();
	APostProcessVolume* PostProcessVolume = TestWorld->SpawnActor<APostProcessVolume>();
	if (!TestNotNull(TEXT("desktop TimeCycleManager"), Manager)
		|| !TestNotNull(TEXT("desktop TimeCycleSky"), TimeCycleSky)
		|| !TestNotNull(TEXT("desktop unbound post-process volume"), PostProcessVolume))
	{
		return false;
	}

	UDirectionalLightComponent* SunLight =
		TimeCycleSky->FindComponentByClass<UDirectionalLightComponent>();
	USkyLightComponent* SkyLight = TimeCycleSky->FindComponentByClass<USkyLightComponent>();
	UReflectionCaptureComponent* E6Component = SpawnProductionE6Capture(
		*this,
		*TestWorld,
		TEXT("CGR_E6_DesktopPreservation"));
	if (!TestNotNull(TEXT("desktop directional light"), SunLight)
		|| !TestNotNull(TEXT("desktop sky light"), SkyLight)
		|| !E6Component)
	{
		return false;
	}

	const FLinearColor DesktopAmbientColor(FColor(61, 127, 193));
	FTimeCycleSkyNightLook DesktopNightLook;
	DesktopNightLook.SkyNightIntensity = 0.63f;
	DesktopNightLook.NightDirectionalIntensity = 0.31f;
	DesktopNightLook.NightDirectionalTemperature = 5400.0f;
	DesktopNightLook.NightAmbientColor = DesktopAmbientColor;
	DesktopNightLook.NightExposureBias = 0.43f;
	if (!TestTrue(
		TEXT("desktop serialized night sentinel is accepted"),
		TimeCycleSky->ApplyTransientNightLook(DesktopNightLook)))
	{
		return false;
	}

	UTexture2D* L2Texture = NewObject<UTexture2D>(TimeCycleSky, TEXT("DesktopTestL2ColorGradingLUT"));
	UTexture2D* DCL2Texture = NewObject<UTexture2D>(TimeCycleSky, TEXT("DesktopTestDCL2ColorGradingLUT"));
	if (!SetSerializedLutReferences(*this, *TimeCycleSky, L2Texture, DCL2Texture))
	{
		return false;
	}

	PostProcessVolume->bUnbound = true;
	PostProcessVolume->Settings.ColorGradingLUT = L2Texture;
	PostProcessVolume->Settings.ColorGradingIntensity = 0.37f;
	PostProcessVolume->Settings.bOverride_ColorGradingLUT = false;
	PostProcessVolume->Settings.bOverride_ColorGradingIntensity = false;

	FTimeCycleCode DayTime;
	DayTime.Hours = 9;
	Manager->SetCurrentTime(DayTime);
	TimeCycleSky->Tick(0.11f);

	TestTrue(
		TEXT("desktop path preserves its LUT"),
		PostProcessVolume->Settings.ColorGradingLUT == L2Texture);
	TestEqual(
		TEXT("desktop path preserves its LUT intensity"),
		PostProcessVolume->Settings.ColorGradingIntensity,
		0.37f,
		ProfileTolerance);
	TestFalse(
		TEXT("desktop path does not enable LUT override"),
		PostProcessVolume->Settings.bOverride_ColorGradingLUT);
	TestFalse(
		TEXT("desktop path does not enable LUT intensity override"),
		PostProcessVolume->Settings.bOverride_ColorGradingIntensity);

	FTimeCycleCode NightTime;
	NightTime.Hours = 0;
	Manager->SetCurrentTime(NightTime);
	TimeCycleSky->Tick(0.11f);
	TestEqual(
		TEXT("desktop Night 00 keeps serialized directional intensity"),
		SunLight->Intensity,
		0.31f,
		ProfileTolerance);
	TestEqual(
		TEXT("desktop Night 00 keeps serialized directional temperature"),
		SunLight->Temperature,
		5400.0f,
		ProfileTolerance);
	TestEqual(
		TEXT("desktop Night 00 keeps serialized skylight intensity"),
		SkyLight->Intensity,
		0.63f,
		ProfileTolerance);
	TestColor(
		*this,
		TEXT("desktop Night 00 keeps serialized ambient color"),
		SkyLight->GetLightColor(),
		DesktopAmbientColor);
	TestEqual(
		TEXT("desktop Night 00 keeps serialized exposure"),
		PostProcessVolume->Settings.AutoExposureBias,
		0.43f,
		ProfileTolerance);
	TestTrue(TEXT("desktop Night 00 keeps E6 reflection visible"), E6Component->IsVisible());
	TestTrue(
		TEXT("desktop Night 00 preserves its LUT"),
		PostProcessVolume->Settings.ColorGradingLUT == L2Texture);
	TestEqual(
		TEXT("desktop Night 00 preserves its LUT intensity"),
		PostProcessVolume->Settings.ColorGradingIntensity,
		0.37f,
		ProfileTolerance);

	FTimeCycleCode DawnTime;
	DawnTime.Hours = 6;
	Manager->SetCurrentTime(DawnTime);
	TimeCycleSky->Tick(0.11f);
	TestEqual(
		TEXT("desktop Dawn keeps serialized directional blend"),
		SunLight->Intensity,
		1.155f,
		ProfileTolerance);
	TestEqual(
		TEXT("desktop Dawn keeps serialized skylight blend"),
		SkyLight->Intensity,
		1.815f,
		ProfileTolerance);
	TestEqual(
		TEXT("desktop Dawn keeps serialized exposure blend"),
		PostProcessVolume->Settings.AutoExposureBias,
		0.14f,
		ProfileTolerance);
	TestTrue(TEXT("desktop Dawn keeps E6 reflection visible"), E6Component->IsVisible());

	FTimeCycleCode DuskTime;
	DuskTime.Hours = 18;
	Manager->SetCurrentTime(DuskTime);
	TimeCycleSky->Tick(0.11f);
	TestEqual(
		TEXT("desktop Dusk keeps serialized directional blend"),
		SunLight->Intensity,
		1.155f,
		ProfileTolerance);
	TestEqual(
		TEXT("desktop Dusk keeps serialized skylight blend"),
		SkyLight->Intensity,
		1.815f,
		ProfileTolerance);
	TestEqual(
		TEXT("desktop Dusk keeps serialized exposure blend"),
		PostProcessVolume->Settings.AutoExposureBias,
		0.14f,
		ProfileTolerance);
	TestTrue(TEXT("desktop Dusk keeps E6 reflection visible"), E6Component->IsVisible());
	TestTrue(
		TEXT("desktop all phases preserve LUT override state"),
		!PostProcessVolume->Settings.bOverride_ColorGradingLUT
			&& !PostProcessVolume->Settings.bOverride_ColorGradingIntensity);

	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRTimeCycleSkyDuskSceneTintTrackingTest,
	"CGR.TimeCycleSky.VisualProfile.DuskSceneTintTracksWarmWeightWhenExposureStalls",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRTimeCycleSkyDuskSceneTintTrackingTest::RunTest(const FString& Parameters)
{
	// SM5 는 ES3_1 전용 모바일 프로파일 분기를 안 타므로 노을 틴트만 독립으로 관찰된다.
	FScopedTimeCycleTestWorld TestWorldScope(ERHIFeatureLevel::SM5);
	UWorld* TestWorld = TestWorldScope.Get();
	if (!TestNotNull(TEXT("SM5 game test world"), TestWorld))
	{
		return false;
	}

	ATimeCycleManager* Manager = TestWorld->SpawnActor<ATimeCycleManager>();
	ATimeCycleSky* TimeCycleSky = TestWorld->SpawnActor<ATimeCycleSky>();
	APostProcessVolume* PostProcessVolume = TestWorld->SpawnActor<APostProcessVolume>();
	if (!TestNotNull(TEXT("dusk tint TimeCycleManager"), Manager)
		|| !TestNotNull(TEXT("dusk tint TimeCycleSky"), TimeCycleSky)
		|| !TestNotNull(TEXT("dusk tint unbound post-process volume"), PostProcessVolume))
	{
		return false;
	}
	PostProcessVolume->bUnbound = true;

	// 긴 황혼 + 완만 램프 — 전환 시작 직후는 램프 기울기가 0이라 노출이 멈춰 있고 노을색만 움직인다.
	if (!SetTimeCycleSkyFloatProperty(*this, *TimeCycleSky, TEXT("DuskBlendHours"), 3.0f)
		|| !SetTimeCycleSkyFloatProperty(*this, *TimeCycleSky, TEXT("PostSunsetTwilightHours"), 1.5f)
		|| !SetTimeCycleSkyFloatProperty(*this, *TimeCycleSky, TEXT("TwilightWarmApex"), 3.0f / 4.5f)
		|| !SetTimeCycleSkyFloatProperty(*this, *TimeCycleSky, TEXT("DuskSceneTintStrength"), 1.0f)
		|| !SetTimeCycleSkyBoolProperty(*this, *TimeCycleSky, TEXT("bSmoothTwilightRamp"), true)
		|| !SetTimeCycleSkyBoolProperty(*this, *TimeCycleSky, TEXT("bDuskSceneTint"), true))
	{
		return false;
	}

	FTimeCycleCode DuskStartTime;
	DuskStartTime.Hours = 16;
	Manager->SetCurrentTime(DuskStartTime);
	TimeCycleSky->Tick(0.11f);

	const float BaselineExposureBias = PostProcessVolume->Settings.AutoExposureBias;
	const FLinearColor BaselineTint = PostProcessVolume->Settings.SceneColorTint;
	TestTrue(
		TEXT("황혼 시작에는 노을 가중치가 0이라 틴트가 흰색"),
		BaselineTint.Equals(FLinearColor::White, ProfileTolerance));

	// 1분 뒤: 노출 변화는 약 1.4e-5 로 0.001 문턱 아래, 노을 가중치는 약 0.0087 로 문턱 위다.
	FTimeCycleCode OneMinuteLater;
	OneMinuteLater.Hours = 16;
	OneMinuteLater.Minutes = 1;
	Manager->SetCurrentTime(OneMinuteLater);
	TimeCycleSky->Tick(0.11f);

	TestEqual(
		TEXT("전제: 이 구간에서 노출 게이트는 열리지 않는다"),
		PostProcessVolume->Settings.AutoExposureBias,
		BaselineExposureBias,
		0.000001f);
	// Mutation caught: 노을 틴트를 노출 변화 게이트 안에 두면 이 시점에 흰색으로 멈춘다.
	TestFalse(
		TEXT("노을 틴트는 노출이 멈춘 구간에서도 갱신된다"),
		PostProcessVolume->Settings.SceneColorTint.Equals(BaselineTint, ProfileTolerance));
	TestTrue(
		TEXT("SceneColorTint override 가 켜진다"),
		PostProcessVolume->Settings.bOverride_SceneColorTint);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRTimeCycleSkyTwilightApexHourTest,
	"CGR.TimeCycleSky.VisualProfile.TwilightApexHourDerivesFromKnobs",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRTimeCycleSkyTwilightApexHourTest::RunTest(const FString& Parameters)
{
	// 치트 Dusk/Dawn 이 점프할 시각. 상수로 박으면 한쪽 맵에서는 밤을 찍는다.
	FScopedTimeCycleTestWorld TestWorldScope(ERHIFeatureLevel::SM5);
	UWorld* TestWorld = TestWorldScope.Get();
	if (!TestNotNull(TEXT("apex test world"), TestWorld))
	{
		return false;
	}

	ATimeCycleManager* Manager = TestWorld->SpawnActor<ATimeCycleManager>();
	ATimeCycleSky* TimeCycleSky = TestWorld->SpawnActor<ATimeCycleSky>();
	if (!TestNotNull(TEXT("apex TimeCycleManager"), Manager)
		|| !TestNotNull(TEXT("apex TimeCycleSky"), TimeCycleSky))
	{
		return false;
	}

	// MainMap 배치형: 황혼 2h · 꼬리 없음 · 대칭 apex -> 정점은 황혼 중앙 18:00
	if (!SetTimeCycleSkyFloatProperty(*this, *TimeCycleSky, TEXT("DuskBlendHours"), 2.0f)
		|| !SetTimeCycleSkyFloatProperty(*this, *TimeCycleSky, TEXT("PostSunsetTwilightHours"), 0.0f)
		|| !SetTimeCycleSkyFloatProperty(*this, *TimeCycleSky, TEXT("TwilightWarmApex"), 0.5f))
	{
		return false;
	}
	TestEqual(
		TEXT("MainMap 배치형 노을 정점은 18:00"),
		TimeCycleSky->GetTwilightApexHour(false),
		18.0f,
		0.001f);

	// WP_Office 배치형: 황혼 3h · 꼬리 1.5h · apex 3/4.5 -> 정점이 일몰 19:00 과 일치
	if (!SetTimeCycleSkyFloatProperty(*this, *TimeCycleSky, TEXT("DuskBlendHours"), 3.0f)
		|| !SetTimeCycleSkyFloatProperty(*this, *TimeCycleSky, TEXT("PostSunsetTwilightHours"), 1.5f)
		|| !SetTimeCycleSkyFloatProperty(*this, *TimeCycleSky, TEXT("TwilightWarmApex"), 3.0f / 4.5f))
	{
		return false;
	}
	TestEqual(
		TEXT("WP_Office 배치형 노을 정점은 해가 지평선에 닿는 19:00"),
		TimeCycleSky->GetTwilightApexHour(false),
		19.0f,
		0.001f);

	// 정점에서 실제로 노을 가중치가 최대인지 — 파생식과 조명 커브가 같은 곳을 가리켜야 한다.
	TimeCycleSkyLighting::FParameters ApexParameters;
	ApexParameters.SunRiseHour = 7.0f;
	ApexParameters.SunSetHour = 19.0f;
	ApexParameters.DuskBlendHours = 3.0f;
	ApexParameters.PostSunsetTwilightHours = 1.5f;
	ApexParameters.TwilightWarmApex = 3.0f / 4.5f;
	ApexParameters.CurrentHour = 19.0f;
	TestEqual(
		TEXT("파생된 정점 시각에서 노을 가중치가 1.0"),
		TimeCycleSkyLighting::Calculate(ApexParameters).WarmWeight,
		1.0f,
		0.001f);

	// 꼬리가 자정을 넘으려 하면 Calculate 와 같은 클램프를 써야 한다.
	TestEqual(
		TEXT("꼬리 클램프는 Calculate 와 동일"),
		TimeCycleSkyLighting::ClampDuskTailHours(3.0f, 23.5f),
		0.49f,
		0.001f);

	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRTimeCycleSkyAtmosphereHorizonContinuityTest,
	"CGR.TimeCycleSky.Lighting.AtmosphereHorizonContinuity",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter | EAutomationTestFlags::NonNullRHI)

bool FCGRTimeCycleSkyAtmosphereHorizonContinuityTest::RunTest(const FString& Parameters)
{
	const ERHIFeatureLevel::Type FeatureLevels[] = {
		ERHIFeatureLevel::SM5,
		ERHIFeatureLevel::ES3_1
	};
	for (ERHIFeatureLevel::Type FeatureLevel : FeatureLevels)
	{
		FScopedTimeCycleTestWorld TestWorldScope(FeatureLevel);
		UWorld* TestWorld = TestWorldScope.Get();
		if (!TestNotNull(TEXT("atmosphere continuity world"), TestWorld))
		{
			return false;
		}

		ATimeCycleManager* Manager = TestWorld->SpawnActor<ATimeCycleManager>();
		ATimeCycleSky* TimeCycleSky = TestWorld->SpawnActor<ATimeCycleSky>();
		ASkyAtmosphere* Atmosphere = TestWorld->SpawnActor<ASkyAtmosphere>();
		if (!TestNotNull(TEXT("continuity clock"), Manager)
			|| !TestNotNull(TEXT("continuity sky"), TimeCycleSky)
			|| !TestNotNull(TEXT("continuity atmosphere"), Atmosphere))
		{
			return false;
		}

		UDirectionalLightComponent* DirectionalLight =
			TimeCycleSky->FindComponentByClass<UDirectionalLightComponent>();
		USkyAtmosphereComponent* AtmosphereComponent = Atmosphere->GetComponent();
		if (!TestNotNull(TEXT("continuity directional light"), DirectionalLight)
			|| !TestNotNull(TEXT("continuity atmosphere component"), AtmosphereComponent))
		{
			return false;
		}

		TimeCycleSky->SetActorRotation(FRotator(0.0f, 37.0f, 0.0f));
		if (!SetTimeCycleSkyFloatProperty(*this, *TimeCycleSky, TEXT("DuskBlendHours"), 3.0f)
			|| !SetTimeCycleSkyFloatProperty(*this, *TimeCycleSky, TEXT("PostSunsetTwilightHours"), 1.5f))
		{
			return false;
		}
		const int32 Times[][2] = {
			{6, 59}, {7, 0}, {7, 1}, {18, 59}, {19, 0}, {19, 1}, {20, 30}, {0, 0}
		};
		FVector PreviousDirection = FVector::ZeroVector;
		for (int32 TimeIndex = 0; TimeIndex < UE_ARRAY_COUNT(Times); ++TimeIndex)
		{
			FTimeCycleCode Time;
			Time.Hours = Times[TimeIndex][0];
			Time.Minutes = Times[TimeIndex][1];
			Manager->SetCurrentTime(Time);
			TimeCycleSky->Tick(0.11f);

			const FString Label = FString::Printf(
				TEXT("feature %d %02d:%02d"), FeatureLevel, Time.Hours, Time.Minutes);
			TestEqual(Label + TEXT(" keeps the atmosphere throughout twilight"),
				DirectionalLight->IsUsedAsAtmosphereSunLight(), TimeIndex < 6);
			TestTrue(Label + TEXT(" uses an independent solar direction"),
				AtmosphereComponent->IsAtmosphereLightDirectionOverriden(0));

			const FVector Direction = AtmosphereComponent->GetOverridenAtmosphereLightDirection(0);
			if (TimeIndex == 0 || TimeIndex >= 5)
			{
				TestTrue(Label + TEXT(" keeps the atmosphere sun below the horizon"), Direction.Z < 0.0);
			}
			else if (TimeIndex == 2 || TimeIndex == 3)
			{
				TestTrue(Label + TEXT(" keeps the atmosphere sun above the horizon"), Direction.Z > 0.0);
			}
			else
			{
				TestTrue(Label + TEXT(" places the atmosphere sun at the horizon"),
					FMath::IsNearlyZero(Direction.Z, 0.0001));
				const FVector ExpectedHorizonDirection = TimeIndex == 1
					? FVector(-0.309016994, -0.951056516, 0.0)
					: FVector(0.309016994, 0.951056516, 0.0);
				TestTrue(Label + TEXT(" includes the sky actor rotation"),
					Direction.Equals(ExpectedHorizonDirection, 0.0001));
			}

			if (TimeIndex == 1 || TimeIndex == 2 || TimeIndex == 4 || TimeIndex == 5)
			{
				TestTrue(Label + TEXT(" changes continuously across the horizon"),
					FVector::Distance(Direction, PreviousDirection) < 0.02);
			}
			if (TimeIndex == 6)
			{
				TestEqual(Label + TEXT(" switches the atmosphere at exactly zero intensity"),
					DirectionalLight->Intensity, 0.0f);
			}
			if (TimeIndex == 7)
			{
				TestTrue(Label + TEXT(" preserves the overhead night fill"),
					DirectionalLight->GetForwardVector().Z < -0.99);
				TestEqual(Label + TEXT(" restores the deep-night fill"), DirectionalLight->Intensity,
					FeatureLevel == ERHIFeatureLevel::ES3_1 ? 0.07f : 0.08f, 0.0001f);
			}
			PreviousDirection = Direction;
		}
		FTimeCycleCode DawnStartTime;
		DawnStartTime.Hours = 5;
		Manager->SetCurrentTime(DawnStartTime);
		TimeCycleSky->Tick(0.11f);
		TestFalse(TEXT("dawn starts with the atmosphere disabled"), DirectionalLight->IsUsedAsAtmosphereSunLight());
		TestEqual(TEXT("dawn registration boundary has exactly zero intensity"), DirectionalLight->Intensity, 0.0f);
	}
	return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS
