#include "Misc/AutomationTest.h"
#include "Utils/CGQABridge.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRQAReflectionBuildPolicyAllowsExactMapsTest,
	"CGR.QA.ReflectionBuildPolicy.AllowsExactMaps",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRQAReflectionBuildPolicyAllowsExactMapsTest::RunTest(const FString& Parameters)
{
	TestTrue(
		TEXT("transient E6 exact package is allowed"),
		CGQABridgePolicy::IsReflectionBuildMapAllowed(
			TEXT("/Game/__CGRTransient/MobileVisualAB/E6/MainMap_TheRiverwalkCity_E6")));
	TestTrue(
		TEXT("production MainMap exact package is allowed"),
		CGQABridgePolicy::IsReflectionBuildMapAllowed(
			TEXT("/Game/CompanyGrowth/Level/MainMap_TheRiverwalkCity")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRQAReflectionBuildPolicyRejectsOtherMapsTest,
	"CGR.QA.ReflectionBuildPolicy.RejectsOtherMaps",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRQAReflectionBuildPolicyRejectsOtherMapsTest::RunTest(const FString& Parameters)
{
	const TArray<FString> RejectedMapPackages = {
		TEXT(""),
		TEXT("/Game/__CGRTransient/MobileVisualAB/E6/MainMap_TheRiverwalkCity"),
		TEXT("/Game/__CGRTransient/MobileVisualAB/E6/MainMap_TheRiverwalkCity_E6_Copy"),
		TEXT("/Game/CompanyGrowth/Level/MainMap_TheRiverwalk"),
		TEXT("/Game/CompanyGrowth/Level/MainMap_TheRiverwalkCity_Copy"),
		TEXT("/Game/CompanyGrowth/Level/Preview_MainMap_TheRiverwalkCity"),
		TEXT("/Game/CompanyGrowth/Level/MainMap_AnotherCity"),
		TEXT("/Game/CompanyGrowth/Level/MainMap_TheRiverwalkCity.MainMap_TheRiverwalkCity"),
	};

	for (const FString& RejectedMapPackage : RejectedMapPackages)
	{
		TestFalse(
			*FString::Printf(TEXT("non-allowlisted package is rejected: '%s'"), *RejectedMapPackage),
			CGQABridgePolicy::IsReflectionBuildMapAllowed(RejectedMapPackage));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRQAMainMapVisualProductionFeatureLevelPolicyTest,
	"CGR.QA.MainMapVisualProduction.FeatureLevelPolicy",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRQAMainMapVisualProductionFeatureLevelPolicyTest::RunTest(const FString& Parameters)
{
	TestFalse(
		TEXT("ES3.1 editor world is rejected before production mutation"),
		CGQABridgePolicy::IsMainMapVisualProductionFeatureLevelSupported(
			ERHIFeatureLevel::ES3_1));
	TestTrue(
		TEXT("SM5 editor world is accepted for production mutation"),
		CGQABridgePolicy::IsMainMapVisualProductionFeatureLevelSupported(
			ERHIFeatureLevel::SM5));
	TestTrue(
		TEXT("SM6 editor world is accepted for production mutation"),
		CGQABridgePolicy::IsMainMapVisualProductionFeatureLevelSupported(
			ERHIFeatureLevel::SM6));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRQAMainMapVisualProductionWorldReadinessPhasePolicyTest,
	"CGR.QA.MainMapVisualProduction.WorldReadinessPhasePolicy",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRQAMainMapVisualProductionWorldReadinessPhasePolicyTest::RunTest(
	const FString& Parameters)
{
	TestFalse(
		TEXT("PostEditorTick readiness waits for the end-of-frame queue to drain"),
		CGQABridgePolicy::IsMainMapVisualProductionWorldReadyForPhase(
			false, true, false, true, true));
	TestTrue(
		TEXT("BeginFrame readiness ignores the newly populated end-of-frame queue"),
		CGQABridgePolicy::IsMainMapVisualProductionWorldReadyForPhase(
			false, true, false, true, false));
	TestFalse(
		TEXT("BeginFrame readiness still rejects async loading"),
		CGQABridgePolicy::IsMainMapVisualProductionWorldReadyForPhase(
			true, true, false, false, false));
	TestFalse(
		TEXT("BeginFrame readiness still rejects a missing expected streaming level"),
		CGQABridgePolicy::IsMainMapVisualProductionWorldReadyForPhase(
			false, false, false, false, false));
	TestFalse(
		TEXT("BeginFrame readiness still rejects pending streaming state"),
		CGQABridgePolicy::IsMainMapVisualProductionWorldReadyForPhase(
			false, true, true, false, false));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
