#include "Misc/AutomationTest.h"
#include "Entity/Ambient/VacantPlotDressingRules.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRVacantPlotDressingRulesTest,
	"CGR.CityDressing.Rules",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRVacantPlotDressingRulesTest::RunTest(const FString& Parameters)
{
	const FVacantPlotFootprint Patch(FVector2D::ZeroVector, FVector2D(100.f, 50.f));

	TestFalse(TEXT("Rectangles that only touch at an edge remain independently visible"),
		FVacantPlotDressingRules::Overlaps(
			Patch,
			FVacantPlotFootprint(FVector2D(200.f, 0.f), FVector2D(100.f, 50.f)),
			/*SafetyPadding=*/0.f));

	TestTrue(TEXT("The 75 cm visual safety margin hides a near building patch"),
		FVacantPlotDressingRules::Overlaps(
			Patch,
			FVacantPlotFootprint(FVector2D(250.f, 0.f), FVector2D(100.f, 50.f)),
			/*SafetyPadding=*/75.f));

	const TArray<FVacantPlotFootprint> Occupancies = {
		FVacantPlotFootprint(FVector2D(800.f, 800.f), FVector2D(50.f, 50.f)),
		FVacantPlotFootprint(FVector2D(250.f, 0.f), FVector2D(100.f, 50.f))
	};
	TestTrue(TEXT("Any overlapping building or preview footprint hides the patch"),
		FVacantPlotDressingRules::OverlapsAny(Patch, Occupancies, /*SafetyPadding=*/75.f));
	TestFalse(TEXT("An empty occupancy list leaves the patch visible"),
		FVacantPlotDressingRules::OverlapsAny(Patch, TConstArrayView<FVacantPlotFootprint>(), 75.f));

	TestTrue(TEXT("A patch fully inside both plot axes is accepted"),
		FVacantPlotDressingRules::IsInsidePlot(
			FVacantPlotFootprint(FVector2D(550.f, -250.f), FVector2D(100.f, 50.f)),
			FVector2D::ZeroVector,
			FVector2D(700.f, 400.f)));

	TestFalse(TEXT("A patch crossing either plot edge is rejected fail-closed"),
		FVacantPlotDressingRules::IsInsidePlot(
			FVacantPlotFootprint(FVector2D(650.f, -250.f), FVector2D(100.f, 50.f)),
			FVector2D::ZeroVector,
			FVector2D(700.f, 400.f)));

	TestEqual(TEXT("Compact plots receive two patches"),
		FVacantPlotDressingRules::ResolvePatchCount(/*BuildingCapacity=*/6), 2);
	TestEqual(TEXT("Ordinary multi-building plots receive three patches"),
		FVacantPlotDressingRules::ResolvePatchCount(/*BuildingCapacity=*/9), 3);
	TestEqual(TEXT("Mega plots stop at the four-patch density cap"),
		FVacantPlotDressingRules::ResolvePatchCount(/*BuildingCapacity=*/48), 4);

	const uint32 Plot01Hash = FVacantPlotDressingRules::StableHash(TEXT("Plot_01"));
	TestEqual(TEXT("The same PlotId produces the same stable seed"),
		FVacantPlotDressingRules::StableHash(TEXT("Plot_01")), Plot01Hash);
	TestNotEqual(TEXT("Different PlotIds do not collapse to one constant seed"),
		FVacantPlotDressingRules::StableHash(TEXT("Plot_02")), Plot01Hash);

	const int32 FirstSelection = FVacantPlotDressingRules::SelectPresetIndex(TEXT("Plot_01"), 2, 3);
	TestEqual(TEXT("Preset selection is repeatable for a PlotId and patch index"),
		FVacantPlotDressingRules::SelectPresetIndex(TEXT("Plot_01"), 2, 3), FirstSelection);
	TestEqual(TEXT("An empty preset list fails closed"),
		FVacantPlotDressingRules::SelectPresetIndex(TEXT("Plot_01"), 2, 0), INDEX_NONE);

	TestEqual(TEXT("A quarter turn swaps a non-square half extent"),
		FVacantPlotDressingRules::RotateHalfExtent90(FVector2D(120.f, 40.f), 1),
		FVector2D(40.f, 120.f));
	TestEqual(TEXT("A half turn preserves a non-square half extent"),
		FVacantPlotDressingRules::RotateHalfExtent90(FVector2D(120.f, 40.f), 2),
		FVector2D(120.f, 40.f));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
