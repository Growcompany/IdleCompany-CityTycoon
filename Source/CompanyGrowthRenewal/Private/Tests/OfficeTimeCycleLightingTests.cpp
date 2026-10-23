#include "Components/ReflectionCaptureComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/Engine.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SphereReflectionCapture.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureCube.h"
#include "Engine/World.h"
#include "Manager/WindowLightManagerSubsystem.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Misc/AutomationTest.h"
#include "TimeCycle/TimeCycleManager.h"
#include "TimeCycle/TimeCycleSky.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
	class FScopedOfficeLightingWorld
	{
	public:
		FScopedOfficeLightingWorld()
		{
			TestWorld = UWorld::CreateWorld(EWorldType::Game, false,
				MakeUniqueObjectName(GetTransientPackage(), UWorld::StaticClass(), TEXT("OfficeLightingTestWorld")),
				GetTransientPackage(), true, ERHIFeatureLevel::ES3_1);
			if (TestWorld && GEngine)
			{
				GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(TestWorld);
			}
		}
		~FScopedOfficeLightingWorld()
		{
			if (TestWorld)
			{
				if (GEngine) { GEngine->DestroyWorldContext(TestWorld); }
				TestWorld->DestroyWorld(false);
			}
		}
		UWorld* Get() const { return TestWorld; }
	private:
		UWorld* TestWorld = nullptr;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfficeWindowLightInitialSyncTest,
	"CGR.Office.TimeCycle.WindowFirstSync", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOfficeWindowLightInitialSyncTest::RunTest(const FString& Parameters)
{
	UMaterialParameterCollection* WindowMPC = TSoftObjectPtr<UMaterialParameterCollection>(FSoftObjectPath(
		TEXT("/Game/CompanyGrowth/Resources/Materials/MainMap/MPC_WindowLight.MPC_WindowLight"))).LoadSynchronous();
	if (!TestNotNull(TEXT("production window MPC"), WindowMPC)) { return false; }
	for (const FTimeCycleCode InitialHour : { FTimeCycleCode{12,0,0}, FTimeCycleCode{0,0,0}, FTimeCycleCode{6,10,0} })
	{
		FScopedOfficeLightingWorld ScopedWorld;
		UWorld* TestWorld = ScopedWorld.Get();
		if (!TestNotNull(TEXT("test world"), TestWorld)) { return false; }
		UWindowLightManagerSubsystem* WindowManager = TestWorld->GetSubsystem<UWindowLightManagerSubsystem>();
		ATimeCycleManager* Clock = TestWorld->SpawnActor<ATimeCycleManager>();
		UMaterialParameterCollectionInstance* MPCInstance = TestWorld->GetParameterCollectionInstance(WindowMPC);
		if (!TestNotNull(TEXT("window subsystem"), WindowManager) || !TestNotNull(TEXT("clock"), Clock)
			|| !TestNotNull(TEXT("world MPC instance"), MPCInstance)) { return false; }
		MPCInstance->SetScalarParameterValue(TEXT("Night_Intensity"), -10.f);
		WindowManager->Tick(0.f);
		float ActualIntensity = 0.f;
		MPCInstance->GetScalarParameterValue(TEXT("Night_Intensity"), ActualIntensity);
		TestEqual(TEXT("unstarted clock does not overwrite MPC with constructor midnight"), ActualIntensity, -10.f);
		Clock->DispatchBeginPlay();
		Clock->SetCurrentTime(InitialHour);
		WindowManager->Tick(0.f);
		const float Expected = InitialHour.Hours == 12 ? 0.f : InitialHour.Hours == 0 ? 1.2f : 0.5f;
		MPCInstance->GetScalarParameterValue(TEXT("Night_Intensity"), ActualIntensity);
		TestEqual(TEXT("first update writes the current time including initial 0.5"), ActualIntensity, Expected, 0.0001f);
		TestEqual(TEXT("reported intensity matches MPC"), WindowManager->GetCurrentNightIntensity(), ActualIntensity);

		// A replaced clock at the same time must synchronize the world collection again.
		Clock->Destroy();
		Clock = TestWorld->SpawnActor<ATimeCycleManager>();
		Clock->DispatchBeginPlay();
		Clock->SetCurrentTime(InitialHour);
		MPCInstance->SetScalarParameterValue(TEXT("Night_Intensity"), -10.f);
		WindowManager->Tick(0.11f);
		MPCInstance->GetScalarParameterValue(TEXT("Night_Intensity"), ActualIntensity);
		TestEqual(TEXT("replacement weak clock cache forces resynchronization"), ActualIntensity, Expected, 0.0001f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfficeTimeCycleProfileIsolationTest,
	"CGR.Office.TimeCycle.ProfileIsolation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOfficeTimeCycleProfileIsolationTest::RunTest(const FString& Parameters)
{
	FScopedOfficeLightingWorld ScopedWorld;
	UWorld* TestWorld = ScopedWorld.Get();
	if (!TestNotNull(TEXT("test world"), TestWorld)) { return false; }
	ATimeCycleSky* Sky = TestWorld->SpawnActor<ATimeCycleSky>();
	ATimeCycleManager* Clock = TestWorld->SpawnActor<ATimeCycleManager>();
	APostProcessVolume* PPV = TestWorld->SpawnActor<APostProcessVolume>();
	ASphereReflectionCapture* Capture = TestWorld->SpawnActor<ASphereReflectionCapture>();
	if (!Sky || !Clock || !PPV || !Capture) { AddError(TEXT("lighting actors failed to spawn")); return false; }
	FBoolProperty* ProfileProperty = FindFProperty<FBoolProperty>(ATimeCycleSky::StaticClass(), TEXT("bApplyMainMapVisualProfile"));
	if (!TestNotNull(TEXT("profile opt-out property"), ProfileProperty)) { return false; }
	TestTrue(TEXT("MainMap retains enabled default"), ProfileProperty->GetPropertyValue_InContainer(Sky));
	ProfileProperty->SetPropertyValue_InContainer(Sky, false);
	UTexture2D* TestLUT = NewObject<UTexture2D>(Sky);
	for (const FName PropertyName : { FName(TEXT("L2ColorGradingLUT")), FName(TEXT("DCL2ColorGradingLUT")) })
	{
		FObjectPropertyBase* LUTProperty = FindFProperty<FObjectPropertyBase>(ATimeCycleSky::StaticClass(), PropertyName);
		if (!TestNotNull(TEXT("LUT property"), LUTProperty)) { return false; }
		LUTProperty->SetObjectPropertyValue_InContainer(Sky, TestLUT);
	}
	UReflectionCaptureComponent* CaptureComponent = Capture->GetCaptureComponent();
	CaptureComponent->ReflectionSourceType = EReflectionSourceType::SpecifiedCubemap;
	CaptureComponent->Cubemap = TSoftObjectPtr<UTextureCube>(FSoftObjectPath(
		TEXT("/Game/CompanyGrowth/Resources/Materials/MainMap/TC_StudioReflect.TC_StudioReflect"))).LoadSynchronous();
	if (!TestNotNull(TEXT("production E6 cubemap"), CaptureComponent->Cubemap.Get())) { return false; }
	CaptureComponent->SetVisibility(true);
	PPV->bUnbound = true;
	PPV->Settings.BloomIntensity = 0.37f;
	PPV->Settings.bOverride_BloomIntensity = true;
	PPV->Settings.ColorGradingIntensity = 0.42f;
	PPV->Settings.bOverride_ColorGradingLUT = false;
	PPV->Settings.ColorGain = FVector4(0.9f, 0.8f, 0.7f, 1.f);
	const FVector4 OriginalGain = PPV->Settings.ColorGain;
	for (const FTimeCycleCode CurrentHour : { FTimeCycleCode{12,0,0}, FTimeCycleCode{0,0,0} })
	{
		Clock->SetCurrentTime(CurrentHour);
		Sky->Tick(0.11f);
		TestEqual(TEXT("Office bloom preserved"), PPV->Settings.BloomIntensity, 0.37f);
		TestEqual(TEXT("Office grading strength preserved"), PPV->Settings.ColorGradingIntensity, 0.42f);
		TestFalse(TEXT("Office LUT override preserved"), PPV->Settings.bOverride_ColorGradingLUT);
		TestTrue(TEXT("Office color gain preserved"), PPV->Settings.ColorGain.Equals(OriginalGain));
		TestTrue(TEXT("Office E6 capture visibility preserved at day and night"), CaptureComponent->IsVisible());
		const bool bDay = CurrentHour.Hours == 12;
		const float ExpectedExposure = bDay ? Sky->GetDayLook().DayExposureBias : Sky->GetNightLook().NightExposureBias;
		TestEqual(TEXT("exposure continues following clock"), PPV->Settings.AutoExposureBias, ExpectedExposure);
		UDirectionalLightComponent* Sun = Sky->FindComponentByClass<UDirectionalLightComponent>();
		USkyLightComponent* Ambient = Sky->FindComponentByClass<USkyLightComponent>();
		if (!Sun || !Ambient) { AddError(TEXT("light components missing")); return false; }
		TestEqual(TEXT("directional lighting continues following clock"), Sun->Intensity,
			bDay ? Sky->GetDayLook().SunMaxIntensity : Sky->GetNightLook().NightDirectionalIntensity);
		TestEqual(TEXT("ambient lighting continues following clock"), Ambient->Intensity,
			bDay ? Sky->GetDayLook().SkyDayIntensity : Sky->GetNightLook().SkyNightIntensity);
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfficeTimeCycleFogTintTest,
	"CGR.Office.TimeCycle.FogTintIsolation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOfficeTimeCycleFogTintTest::RunTest(const FString& Parameters)
{
	FScopedOfficeLightingWorld ScopedWorld;
	UWorld* TestWorld = ScopedWorld.Get();
	if (!TestNotNull(TEXT("test world"), TestWorld)) { return false; }
	ATimeCycleSky* Sky = TestWorld->SpawnActor<ATimeCycleSky>();
	ATimeCycleManager* Clock = TestWorld->SpawnActor<ATimeCycleManager>();
	AExponentialHeightFog* FogActor = TestWorld->SpawnActor<AExponentialHeightFog>();
	if (!Sky || !Clock || !FogActor) { AddError(TEXT("fog test actors failed to spawn")); return false; }
	UExponentialHeightFogComponent* FogComponent = FogActor->GetComponent();
	const FLinearColor OriginalColor(0.42f, 0.45f, 0.48f);
	const FVector OriginalLocation(200.f, 100.f, -9000.f);
	FogComponent->SetFogInscatteringColor(OriginalColor);
	FogComponent->SetFogDensity(0.08f);
	FogComponent->SetWorldLocation(OriginalLocation);
	FogComponent->SetVisibility(false);
	Clock->SetCurrentTime(FTimeCycleCode{0,0,0});
	Sky->Tick(0.11f);
	TestTrue(TEXT("default disabled leaves MainMap fog untouched"), FogComponent->FogInscatteringLuminance.Equals(OriginalColor));
	FBoolProperty* TintProperty = FindFProperty<FBoolProperty>(ATimeCycleSky::StaticClass(), TEXT("bApplyOfficeFogTint"));
	if (!TestNotNull(TEXT("Office fog opt-in property"), TintProperty)) { return false; }
	TintProperty->SetPropertyValue_InContainer(Sky, true);
	Sky->Tick(0.11f);
	const FLinearColor NightColor(0.012f, 0.020f, 0.038f);
	TestTrue(TEXT("Office midnight uses restrained blue fog"), FogComponent->FogInscatteringLuminance.Equals(NightColor));
	Clock->SetCurrentTime(FTimeCycleCode{6,0,0});
	Sky->Tick(0.11f);
	TestTrue(TEXT("dawn fog follows shared day alpha"), FogComponent->FogInscatteringLuminance.Equals(FMath::Lerp(NightColor, OriginalColor, 0.5f)));
	Clock->SetCurrentTime(FTimeCycleCode{12,0,0});
	Sky->Tick(0.11f);
	TestTrue(TEXT("noon restores cached original daytime color"), FogComponent->FogInscatteringLuminance.Equals(OriginalColor));
	TestEqual(TEXT("fog tint preserves shell density"), FogComponent->FogDensity, 0.08f);
	TestTrue(TEXT("fog tint preserves shell height"), FogComponent->GetComponentLocation().Equals(OriginalLocation));
	TestFalse(TEXT("fog tint preserves shell visibility"), FogComponent->IsVisible());
	Clock->SetCurrentTime(FTimeCycleCode{0,0,0});
	Sky->Tick(0.11f);
	TintProperty->SetPropertyValue_InContainer(Sky, false);
	Sky->Tick(0.11f);
	TestTrue(TEXT("disabling tint restores original color"), FogComponent->FogInscatteringLuminance.Equals(OriginalColor));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfficeDaylightCaptureScopeTest,
	"CGR.Office.TimeCycle.DaylightCaptureScope", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FOfficeDaylightCaptureScopeTest::RunTest(const FString& Parameters)
{
	FScopedOfficeLightingWorld ScopedWorld;
	UWorld* TestWorld = ScopedWorld.Get();
	if (!TestNotNull(TEXT("test world"), TestWorld)) { return false; }
	ATimeCycleSky* Sky = TestWorld->SpawnActor<ATimeCycleSky>();
	ATimeCycleManager* Clock = TestWorld->SpawnActor<ATimeCycleManager>();
	APostProcessVolume* PPV = TestWorld->SpawnActor<APostProcessVolume>();
	AExponentialHeightFog* Fog = TestWorld->SpawnActor<AExponentialHeightFog>();
	if (!Sky || !Clock || !PPV || !Fog) { return false; }
	UDirectionalLightComponent* Sun = Sky->FindComponentByClass<UDirectionalLightComponent>();
	USkyLightComponent* Ambient = Sky->FindComponentByClass<USkyLightComponent>();
	UMaterialParameterCollection* WindowMPC = TSoftObjectPtr<UMaterialParameterCollection>(FSoftObjectPath(
		TEXT("/Game/CompanyGrowth/Resources/Materials/MainMap/MPC_WindowLight.MPC_WindowLight"))).LoadSynchronous();
	UMaterialParameterCollectionInstance* MPC = TestWorld->GetParameterCollectionInstance(WindowMPC);
	FStructProperty* ClockProperty = FindFProperty<FStructProperty>(ATimeCycleManager::StaticClass(), TEXT("TimeCycleData"));
	if (!Sun || !Ambient || !MPC || !ClockProperty) { return false; }
	FTimeCycleData& Data = *ClockProperty->ContainerPtrToValuePtr<FTimeCycleData>(Clock);
	Data.CurrentTime = 76500.375f;
	Data.NumCycles = 7;
	Data.bIsCyclePaused = false;
	const FLinearColor AuthoredDayFogColor(0.6f, 0.7f, 0.8f);
	UExponentialHeightFogComponent* FogComponent = Fog->GetComponent();
	FogComponent->SetFogInscatteringColor(AuthoredDayFogColor);
	FogComponent->SetWorldLocation(FVector(13.f, 27.f, -4500.f));
	FBoolProperty* TintProperty = FindFProperty<FBoolProperty>(ATimeCycleSky::StaticClass(), TEXT("bApplyOfficeFogTint"));
	if (!TestNotNull(TEXT("Office fog tint policy"), TintProperty)) { return false; }
	TintProperty->SetPropertyValue_InContainer(Sky, true);
	Sky->Tick(0.11f);
	const FLinearColor SavedNightFogColor = FogComponent->FogInscatteringLuminance;
	TestFalse(TEXT("night entry fixture has nighttime fog"), SavedNightFogColor.Equals(AuthoredDayFogColor));
	const FTransform SavedFogTransform = FogComponent->GetComponentTransform();
	Sun->SetRelativeRotation(FRotator(-27, 81, 13));
	Sun->SetIntensity(0.17f);
	Sun->SetTemperature(8900.f);
	Sun->SetUseTemperature(false);
	Sun->SetLightColor(FLinearColor(0.3f, 0.6f, 0.8f));
	Ambient->SetIntensity(0.23f);
	Ambient->SetLightColor(FLinearColor(0.1f, 0.2f, 0.7f));
	const FTransform SavedTransform = Sun->GetRelativeTransform();
	const FColor SavedSunColor = Sun->LightColor;
	const FColor SavedSkyColor = Ambient->LightColor;
	MPC->SetScalarParameterValue(TEXT("Night_Intensity"), 1.2f);
	PPV->Settings.AutoExposureBias = 0.67f;
	Fog->GetComponent()->SetFogDensity(0.031f);
	const bool bResult = Sky->WithDaylightForCapture([&]()
	{
		TestEqual(TEXT("capture uses existing authored daytime sun"), Sun->Intensity, Sky->GetDayLook().SunMaxIntensity);
		TestEqual(TEXT("capture uses existing authored daytime ambient"), Ambient->Intensity, Sky->GetDayLook().SkyDayIntensity);
		float Night = -1.f;
		MPC->GetScalarParameterValue(TEXT("Night_Intensity"), Night);
		TestEqual(TEXT("day capture suppresses window night emission"), Night, 0.f);
		TestEqual(TEXT("fractional game time never changes during capture"), Data.CurrentTime, 76500.375f);
		TestTrue(TEXT("night entry captures authored daytime height fog"), FogComponent->FogInscatteringLuminance.Equals(AuthoredDayFogColor));
		TestTrue(TEXT("capture keeps real fog height and transform"), FogComponent->GetComponentTransform().Equals(SavedFogTransform));
		TestEqual(TEXT("capture keeps real fog density"), FogComponent->FogDensity, 0.031f);
		return false;
	});
	TestFalse(TEXT("capture failure is returned"), bResult);
	TestTrue(TEXT("sun transform restored on capture failure"), Sun->GetRelativeTransform().Equals(SavedTransform));
	TestEqual(TEXT("sun intensity restored"), Sun->Intensity, 0.17f);
	TestEqual(TEXT("sun temperature restored"), Sun->Temperature, 8900.f);
	TestFalse(TEXT("temperature enable restored"), Sun->bUseTemperature);
	TestEqual(TEXT("sun color restored exactly"), Sun->LightColor, SavedSunColor);
	TestEqual(TEXT("ambient color restored exactly"), Ambient->LightColor, SavedSkyColor);
	TestEqual(TEXT("ambient intensity restored"), Ambient->Intensity, 0.23f);
	float Night = -1.f;
	MPC->GetScalarParameterValue(TEXT("Night_Intensity"), Night);
	TestEqual(TEXT("night MPC restored on failure"), Night, 1.2f);
	TestEqual(TEXT("clock seconds including fraction unchanged"), Data.CurrentTime, 76500.375f);
	TestEqual(TEXT("cycle count unchanged"), Data.NumCycles, 7);
	TestFalse(TEXT("clock pause state unchanged"), Data.bIsCyclePaused);
	TestEqual(TEXT("post process exposure untouched"), PPV->Settings.AutoExposureBias, 0.67f);
	TestEqual(TEXT("fog untouched"), Fog->GetComponent()->FogDensity, 0.031f);
	TestTrue(TEXT("night fog color restored exactly after failed capture"), FogComponent->FogInscatteringLuminance.Equals(SavedNightFogColor));
	TestTrue(TEXT("fog transform restored unchanged"), FogComponent->GetComponentTransform().Equals(SavedFogTransform));
	return true;
}
#endif
