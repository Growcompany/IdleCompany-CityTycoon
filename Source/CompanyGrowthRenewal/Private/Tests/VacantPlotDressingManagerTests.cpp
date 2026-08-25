#include "Misc/AutomationTest.h"
#include "Entity/Ambient/VacantPlotDressingManager.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRVacantPlotDressingManagerTest,
	"CGR.CityDressing.ManagerVisibility",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRVacantPlotDressingManagerTest::RunTest(const FString& Parameters)
{
	const FVacantPlotFootprint Patch(FVector2D(100.f, 100.f), FVector2D(120.f, 60.f));

	TestTrue(TEXT("A patch with no building or preview occupancy remains visible"),
		AVacantPlotDressingManager::ShouldPatchBeVisible(
			Patch, TConstArrayView<FVacantPlotFootprint>(), 75.f));

	const TArray<FVacantPlotFootprint> FarOccupancy = {
		FVacantPlotFootprint(FVector2D(1000.f, 1000.f), FVector2D(100.f, 100.f))
	};
	TestTrue(TEXT("A distant building leaves the patch visible"),
		AVacantPlotDressingManager::ShouldPatchBeVisible(Patch, FarOccupancy, 75.f));

	const TArray<FVacantPlotFootprint> PreviewOverlap = {
		FVacantPlotFootprint(FVector2D(330.f, 100.f), FVector2D(40.f, 40.f))
	};
	TestFalse(TEXT("The active preview hides only its intersecting patch"),
		AVacantPlotDressingManager::ShouldPatchBeVisible(Patch, PreviewOverlap, 75.f));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
