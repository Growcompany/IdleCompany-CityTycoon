#include "Misc/AutomationTest.h"
#include "Player/Components/PlotPlacementRules.h"
#include "Table/BuildingData.h"

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

	// A high camera focus projects the viewport center onto a distant ground plot.
	// Initial placement stays near the focus XY, and the model pivot offset cannot bias that search.
	const FVector CameraFocus(400.0, 200.0, 16900.0);
	const FVector ProjectedGround(6400.0, 200.0, 10900.0);
	const FVector OffPivotOffset(866.0, 0.0, 0.0);
	const FVector InitialPivot = FPlotPlacementRules::ResolveInitialPlacementPosition(
		CameraFocus, ProjectedGround, OffPivotOffset);
	TestEqual(TEXT("Initial actor pivot compensates the displaced body center"),
		InitialPivot, FVector(-466.0, 200.0, 10900.0));
	const FVector InitialCenter = InitialPivot + OffPivotOffset;
	const TArray<FVector2D> NearbyAndDistantCandidates = {
		FVector2D(500.0, 200.0), FVector2D(6200.0, 200.0)
	};
	const TArray<uint8> BothValid = { 1, 1 };
	TestEqual(TEXT("A high camera focus selects the nearby plot instead of the distant ray intersection"),
		FPlotPlacementRules::FindNearestValidCandidateIndex(
			FVector2D(InitialCenter.X, InitialCenter.Y),
			MakeArrayView(NearbyAndDistantCandidates), MakeArrayView(BothValid)), 0);

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRAuthoredPlotGridTest,
	"CGR.Building.AuthoredPlotGrid",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRAuthoredPlotGridTest::RunTest(const FString& Parameters)
{
	constexpr double Cell = 4300.0;

	// FIntPoint 는 LexToString 이 없어 TestEqual 템플릿에 직접 못 넘긴다 — 성분으로 비교한다.
	auto TestGrid = [this](const TCHAR* What, FIntPoint Grid, int32 ExpectedX, int32 ExpectedY)
	{
		TestEqual(FString::Printf(TEXT("%s (cols)"), What), Grid.X, ExpectedX);
		TestEqual(FString::Printf(TEXT("%s (rows)"), What), Grid.Y, ExpectedY);
	};

	TestGrid(TEXT("An exact 3x3 box derives a 3x3 grid"),
		FPlotPlacementRules::DeriveGridFromHalfExtent(6450.0, 6450.0, Cell), 3, 3);

	TestGrid(TEXT("A box one tick short of a cell boundary still derives the full grid"),
		FPlotPlacementRules::DeriveGridFromHalfExtent(6449.9, 6449.9, Cell), 3, 3);

	TestGrid(TEXT("Leftover under a full cell is dropped rather than rounded up"),
		FPlotPlacementRules::DeriveGridFromHalfExtent(8000.0, 6450.0, Cell), 3, 3);

	TestGrid(TEXT("A box narrower than one cell derives zero columns"),
		FPlotPlacementRules::DeriveGridFromHalfExtent(2000.0, 6450.0, Cell), 0, 3);

	TestGrid(TEXT("A non-square box keeps its axes distinct"),
		FPlotPlacementRules::DeriveGridFromHalfExtent(6450.0, 4300.0, Cell), 3, 2);

	TestEqual(TEXT("Capacity is the cell product"),
		FPlotPlacementRules::DeriveCapacityFromGrid(FIntPoint(3, 2)), 6);

	TestEqual(TEXT("A grid with a zero axis has no capacity"),
		FPlotPlacementRules::DeriveCapacityFromGrid(FIntPoint(0, 3)), 0);

	// 스냅은 "그린 만큼 = 수용량"을 보장한다 — 잔여 여백이 남으면 박스와 칸 수가 어긋난다.
	const FVector2D Snapped = FPlotPlacementRules::SnapHalfExtentToCells(8000.0, 6450.0, Cell);
	TestEqual(TEXT("Snapping trims the leftover on the oversized axis"), Snapped.X, 6450.0);
	TestEqual(TEXT("Snapping leaves an already exact axis untouched"), Snapped.Y, 6450.0);

	const FVector2D TooSmall = FPlotPlacementRules::SnapHalfExtentToCells(100.0, 100.0, Cell);
	TestEqual(TEXT("A sub-cell box is left as authored instead of collapsing to zero"), TooSmall.X, 100.0);

	TestGrid(TEXT("A degenerate extent derives no grid"),
		FPlotPlacementRules::DeriveGridFromHalfExtent(0.0, 6450.0, Cell), 0, 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRPlotPaddingTest,
	"CGR.Building.PlotPadding",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRPlotPaddingTest::RunTest(const FString& Parameters)
{
	const double CompactPitch = FPlotPlacementRules::GetPlotCellSize(PlotBaseCellSizeCompact);
	const double CityPitch = FPlotPlacementRules::GetPlotCellSize(PlotBaseCellSizeRiverwalk);
	TestEqual(TEXT("Compact plot retains its 20.4 m pitch when the building cell shrinks"), CompactPitch, 2040.0);
	TestEqual(TEXT("Riverwalk plot retains its independent 43.86 m pitch"), CityPitch, 4386.0);

	// One column, two rows: two current building footprints fit at the authored positions.
	// The lower one crosses the old 2000 cm plot edge, so removing padding breaks this case.
	const double PlotHalfX = CompactPitch * 0.5;
	const double PlotHalfY = CompactPitch;
	constexpr double FootprintHalfSize = BuildingFootprintCellSize * 0.5;
	TestTrue(TEXT("Upper footprint stays inside the padded plot on both axes"),
		FootprintHalfSize <= PlotHalfX && FMath::Abs(900.0) + FootprintHalfSize <= PlotHalfY);
	TestTrue(TEXT("Lower footprint uses the new outer margin on both axes"),
		FootprintHalfSize <= PlotHalfX && FMath::Abs(-1100.0) + FootprintHalfSize <= PlotHalfY);
	TestTrue(TEXT("The two footprints remain separated without intersecting"),
		900.0 - FootprintHalfSize >= -1100.0 + FootprintHalfSize);

	// A rectangular plot must expand both axes by the same ratio without gaining logical cells.
	const FIntPoint LargeGrid(6, 10);
	const double LargeHalfX = LargeGrid.X * CityPitch * 0.5;
	const double LargeHalfY = LargeGrid.Y * CityPitch * 0.5;
	TestEqual(TEXT("Six-column width has one percent beyond each original outer edge"), LargeHalfX, 13158.0);
	TestEqual(TEXT("Ten-row depth has one percent beyond each original outer edge"), LargeHalfY, 21930.0);
	const FIntPoint DerivedGrid = FPlotPlacementRules::DeriveGridFromHalfExtent(LargeHalfX, LargeHalfY, CityPitch);
	TestEqual(TEXT("Padded large plot retains six logical columns"), DerivedGrid.X, LargeGrid.X);
	TestEqual(TEXT("Padded large plot retains ten logical rows"), DerivedGrid.Y, LargeGrid.Y);
	TestEqual(TEXT("Padding does not increase the building count cap"),
		FPlotPlacementRules::DeriveCapacityFromGrid(DerivedGrid), 60);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
