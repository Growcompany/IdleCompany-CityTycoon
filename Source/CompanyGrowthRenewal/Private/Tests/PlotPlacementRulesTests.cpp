#include "Misc/AutomationTest.h"
#include "Player/Components/PlotPlacementRules.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRPlotPlacementRulesTest,
	"CGR.Building.PlotPlacementRules",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRNearestValidPlotPlacementTest,
	"CGR.Building.PlotNearestValidCandidate",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRNearestValidPlotPlacementTest::RunTest(const FString& Parameters)
{
	const FVector2D DesiredPosition(0.f, 0.f);
	const TArray<FVector2D> CandidatePositions = {
		FVector2D(100.f, 0.f),
		FVector2D(900.f, 0.f),
		FVector2D(300.f, 0.f)
	};
	const TArray<uint8> CandidateValidity = { 0, 1, 1 };

	TestEqual(TEXT("The nearest valid candidate is selected even when the closest candidate is invalid"),
		FPlotPlacementRules::FindNearestValidCandidateIndex(
			DesiredPosition,
			MakeArrayView(CandidatePositions),
			MakeArrayView(CandidateValidity)),
		2);

	return true;
}
bool FCGRPlotPlacementRulesTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Moving within the original plot excludes the moving building from capacity"),
		FPlotPlacementRules::HasCapacity(
			/*CurrentBuildingCount=*/2,
			/*BuildingCapacity=*/2,
			/*bExcludeMovingBuilding=*/true));

	TestFalse(TEXT("A full destination plot rejects another building"),
		FPlotPlacementRules::HasCapacity(
			/*CurrentBuildingCount=*/2,
			/*BuildingCapacity=*/2,
			/*bExcludeMovingBuilding=*/false));

	TestTrue(TEXT("A footprint that fits only after a 90 degree turn is a plot candidate"),
		FPlotPlacementRules::CanFootprintFitPlot(
			/*PlotCols=*/2,
			/*PlotRows=*/3,
			/*FootprintWidthCells=*/3,
			/*FootprintDepthCells=*/2));

	TestFalse(TEXT("A footprint larger than both plot orientations is not a plot candidate"),
		FPlotPlacementRules::CanFootprintFitPlot(
			/*PlotCols=*/3,
			/*PlotRows=*/3,
			/*FootprintWidthCells=*/4,
			/*FootprintDepthCells=*/1));

	TestFalse(TEXT("An oversized footprint in the current orientation cannot be placed"),
		FPlotPlacementRules::DoesCurrentOrientationFitWithinPlot(
			/*PlotHalfX=*/6450.f,
			/*PlotHalfY=*/6450.f,
			/*FootprintHalfX=*/8600.f,
			/*FootprintHalfY=*/2150.f));

	TestTrue(TEXT("A footprint inside both current-orientation plot axes can be placed"),
		FPlotPlacementRules::DoesCurrentOrientationFitWithinPlot(
			/*PlotHalfX=*/6450.f,
			/*PlotHalfY=*/6450.f,
			/*FootprintHalfX=*/4300.f,
			/*FootprintHalfY=*/2150.f));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
