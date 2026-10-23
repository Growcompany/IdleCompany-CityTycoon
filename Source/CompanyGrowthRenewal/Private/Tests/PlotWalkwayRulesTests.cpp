#include "Misc/AutomationTest.h"
#include "Entity/Plot/PlotWalkwayRules.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FPlotWalkwayGrid MakePlotWalkwayGrid(int32 Width, int32 Height)
	{
		FPlotWalkwayGrid Grid;
		Grid.Size = FIntPoint(Width, Height);
		Grid.Ground.Init(1, Width * Height);
		return Grid;
	}

	FBox2D MakePlotWalkwayBox(float MinX, float MinY, float MaxX, float MaxY)
	{
		return FBox2D(FVector2D(MinX, MinY), FVector2D(MaxX, MaxY));
	}

	bool OverlapsPlotWalkwayInterior(const FBox2D& Left, const FBox2D& Right)
	{
		return Left.Min.X < Right.Max.X && Right.Min.X < Left.Max.X
			&& Left.Min.Y < Right.Max.Y && Right.Min.Y < Left.Max.Y;
	}

	FPlotWalkwayTarget MakePlotWalkwayTarget(int32 Id, float MinX, float MinY, float MaxX, float MaxY)
	{
		FPlotWalkwayTarget Result;
		Result.Id = Id;
		Result.Bounds = MakePlotWalkwayBox(MinX, MinY, MaxX, MaxY);
		return Result;
	}

	bool IsPlotWalkwaySortedAndUnique(const TArray<int32>& Cells)
	{
		for (int32 Index = 1; Index < Cells.Num(); ++Index)
		{
			if (Cells[Index - 1] >= Cells[Index])
			{
				return false;
			}
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRPlotWalkwaySingleBuildingTest,
	"CGR.CityWalkways.Rules.SingleBuildingEntrance",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRPlotWalkwaySingleBuildingTest::RunTest(const FString& Parameters)
{
	FPlotWalkwayGrid Grid = MakePlotWalkwayGrid(7, 5);
	Grid.EntranceCells = { 14 }; // (0, 2)
	const TArray<FPlotWalkwayTarget> Targets = { MakePlotWalkwayTarget(11, 2000.f, 800.f, 2400.f, 1200.f) };
	const FPlotWalkwayResult Result = FPlotWalkwayRules::Solve(Grid, Targets, TConstArrayView<FBox2D>());
	TestTrue(TEXT("A grounded entrance and building produce a valid route"), Result.bValid);
	TestTrue(TEXT("The building is reachable"), Result.ReachableTargetIds.Contains(11));
	TestEqual(TEXT("No other target is reported"), Result.ReachableTargetIds.Num(), 1);
	TestTrue(TEXT("The route begins at the verified external entrance"), Result.PathCells.Contains(14));
	TestTrue(TEXT("The shortest route can end one cell before the building"), Result.PathCells.Contains(17));
	TestFalse(TEXT("The building footprint is not paved as a route"), Result.PathCells.Contains(19));
	if (TestEqual(TEXT("The gap has one rendered connector"), Result.Connectors.Num(), 1))
	{
		const FPlotWalkwayConnector& Connector = Result.Connectors[0];
		TestEqual(TEXT("The connector belongs to the reached building"), Connector.TargetId, 11);
		TestEqual(TEXT("The connector begins at the selected path cell"), Connector.Cell, 17);
		TestEqual(TEXT("The connector starts at that cell's outside face"), Connector.Bounds.Min,
			FVector2D(1600.f, 800.f));
		TestEqual(TEXT("The connector ends at the target's face with full width"), Connector.Bounds.Max,
			FVector2D(2000.f, 1200.f));
	}
	TestTrue(TEXT("Rendered cells are sorted and unique"), IsPlotWalkwaySortedAndUnique(Result.PathCells));
	const FPlotWalkwayResult ReachabilityOnly = FPlotWalkwayRules::Solve(Grid, Targets,
		TConstArrayView<FBox2D>(), false);
	TestTrue(TEXT("A validation-only solve remains valid"), ReachabilityOnly.bValid);
	TestTrue(TEXT("A validation-only solve reports the same building"),
		ReachabilityOnly.ReachableTargetIds.Contains(11));
	TestTrue(TEXT("A validation-only solve emits no rendered connector"), ReachabilityOnly.Connectors.IsEmpty());
	TestEqual(TEXT("Cell centers use row-major world coordinates"),
		FPlotWalkwayRules::CellCenter(Grid, 14), FVector2D(200.f, 1000.f));
	const FBox2D FirstSquare = FPlotWalkwayRules::CellBounds(Grid, 0);
	TestEqual(TEXT("Cell bounds begin at the declared world origin"), FirstSquare.Min,
		FVector2D::ZeroVector);
	TestEqual(TEXT("A cell covers exactly one 400 cm square"), FirstSquare.Max,
		FVector2D(400.f, 400.f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRPlotWalkwaySharedConnectionTest,
	"CGR.CityWalkways.Rules.SharedEntrance",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRPlotWalkwaySharedConnectionTest::RunTest(const FString& Parameters)
{
	FPlotWalkwayGrid Grid = MakePlotWalkwayGrid(8, 7);
	Grid.EntranceCells = { 31, 24 }; // (7, 3) and (0, 3), deliberately unsorted
	const TArray<FPlotWalkwayTarget> Targets = {
		MakePlotWalkwayTarget(20, 1200.f, 800.f, 1600.f, 1200.f),
		MakePlotWalkwayTarget(10, 1200.f, 1600.f, 1600.f, 2000.f)
	};
	const FPlotWalkwayResult Result = FPlotWalkwayRules::Solve(Grid, Targets, TConstArrayView<FBox2D>());
	TestTrue(TEXT("Two buildings in the same free component are valid"), Result.bValid);
	TestEqual(TEXT("Both buildings are externally reachable"), Result.ReachableTargetIds.Num(), 2);
	TestTrue(TEXT("First building is reached"), Result.ReachableTargetIds.Contains(10));
	TestTrue(TEXT("Second building is reached"), Result.ReachableTargetIds.Contains(20));
	TestTrue(TEXT("The nearest entrance serves this component"), Result.PathCells.Contains(24));
	TestFalse(TEXT("The component does not add the farther external entrance"), Result.PathCells.Contains(31));
	TestEqual(TEXT("Only the selected entrance receives the external apron"),
		Result.UsedEntranceCells.Num(), 1);
	if (Result.UsedEntranceCells.Num() == 1)
	{
		TestEqual(TEXT("The shared route uses entrance 24"), Result.UsedEntranceCells[0], 24);
	}
	TestFalse(TEXT("The upper building cell is not used as a shortcut"), Result.PathCells.Contains(35));
	TestFalse(TEXT("The lower building cell is not used as a shortcut"), Result.PathCells.Contains(19));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRPlotWalkwayBlockedAccessTest,
	"CGR.CityWalkways.Rules.BlockedAndNarrowAccess",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRPlotWalkwayBlockedAccessTest::RunTest(const FString& Parameters)
{
	FPlotWalkwayGrid Grid = MakePlotWalkwayGrid(4, 3);
	Grid.EntranceCells = { 4 }; // (0, 1)
	const TArray<FPlotWalkwayTarget> Targets = { MakePlotWalkwayTarget(9, 1200.f, 400.f, 1600.f, 800.f) };
	const TArray<FBox2D> Wall = { MakePlotWalkwayBox(400.f, 0.f, 800.f, 1200.f) };
	const FPlotWalkwayResult Blocked = FPlotWalkwayRules::Solve(Grid, Targets, Wall);
	TestTrue(TEXT("A blocked but well-formed plot is valid"), Blocked.bValid);
	TestFalse(TEXT("A full wall blocks the target"), Blocked.ReachableTargetIds.Contains(9));
	TestTrue(TEXT("No disconnected paving is emitted"), Blocked.PathCells.IsEmpty());

	const TArray<FBox2D> NarrowGap = {
		MakePlotWalkwayBox(400.f, 0.f, 800.f, 450.f),
		MakePlotWalkwayBox(400.f, 750.f, 800.f, 1200.f)
	};
	const FPlotWalkwayResult Narrow = FPlotWalkwayRules::Solve(Grid, Targets, NarrowGap);
	TestTrue(TEXT("A 300 cm gap is a valid input"), Narrow.bValid);
	TestFalse(TEXT("A gap narrower than the 400 cm square cannot carry the route"),
		Narrow.ReachableTargetIds.Contains(9));

	FPlotWalkwayGrid ConnectorGrid = MakePlotWalkwayGrid(4, 3);
	ConnectorGrid.Ground.Init(0, 12);
	for (int32 X = 0; X < 4; ++X)
	{
		ConnectorGrid.Ground[4 + X] = 1;
	}
	ConnectorGrid.EntranceCells = { 4 };
	const TArray<FPlotWalkwayTarget> RecessedTarget = {
		MakePlotWalkwayTarget(12, 1300.f, 400.f, 1700.f, 800.f)
	};
	const FPlotWalkwayResult ClearConnector = FPlotWalkwayRules::Solve(
		ConnectorGrid, RecessedTarget, TConstArrayView<FBox2D>());
	TestTrue(TEXT("A grounded short connector reaches a recessed target"),
		ClearConnector.ReachableTargetIds.Contains(12));
	const TArray<FBox2D> ConnectorBlocker = { MakePlotWalkwayBox(1200.f, 450.f, 1250.f, 750.f) };
	const FPlotWalkwayResult CoveredConnector = FPlotWalkwayRules::Solve(
		ConnectorGrid, RecessedTarget, ConnectorBlocker);
	TestTrue(TEXT("A blocker in the short connector is a valid input"), CoveredConnector.bValid);
	TestFalse(TEXT("A blocker crossing only the connector still prevents entry"),
		CoveredConnector.ReachableTargetIds.Contains(12));
	const TArray<FPlotWalkwayTarget> NarrowFrontage = {
		MakePlotWalkwayTarget(13, 1300.f, 550.f, 1700.f, 850.f)
	};
	const FPlotWalkwayResult NarrowFace = FPlotWalkwayRules::Solve(
		ConnectorGrid, NarrowFrontage, TConstArrayView<FBox2D>());
	TestFalse(TEXT("A positive gap needs a full 400 cm projected target face"),
		NarrowFace.ReachableTargetIds.Contains(13));

	FPlotWalkwayGrid HoleGrid = MakePlotWalkwayGrid(5, 3);
	HoleGrid.Ground.Init(0, 15);
	HoleGrid.Ground[5] = 1;
	HoleGrid.Ground[6] = 1;
	HoleGrid.Ground[7] = 1;
	HoleGrid.Ground[9] = 1;
	HoleGrid.EntranceCells = { 5 };
	const TArray<FPlotWalkwayTarget> AcrossHole = {
		MakePlotWalkwayTarget(14, 1600.f, 400.f, 2000.f, 800.f)
	};
	const FPlotWalkwayResult MissingConnectorGround = FPlotWalkwayRules::Solve(
		HoleGrid, AcrossHole, TConstArrayView<FBox2D>());
	TestFalse(TEXT("A full-width connector cannot bridge a missing ground cell"),
		MissingConnectorGround.ReachableTargetIds.Contains(14));
	HoleGrid.Ground[8] = 1;
	const TArray<FPlotWalkwayTarget> AcrossBuilding = {
		MakePlotWalkwayTarget(15, 1200.f, 400.f, 1600.f, 800.f),
		MakePlotWalkwayTarget(16, 1600.f, 400.f, 2000.f, 800.f)
	};
	const FPlotWalkwayResult BuildingInConnector = FPlotWalkwayRules::Solve(
		HoleGrid, AcrossBuilding, TConstArrayView<FBox2D>());
	TestFalse(TEXT("A connector cannot cross another target building"),
		BuildingInConnector.ReachableTargetIds.Contains(16));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRPlotWalkwayGroundMaskTest,
	"CGR.CityWalkways.Rules.GroundMaskAndCardinalTravel",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRPlotWalkwayGroundMaskTest::RunTest(const FString& Parameters)
{
	FPlotWalkwayGrid Grid = MakePlotWalkwayGrid(3, 3);
	Grid.Ground.Init(0, 9);
	Grid.Ground[0] = 1;
	Grid.Ground[4] = 1; // Diagonal contact alone does not connect cells.
	Grid.Ground[5] = 1; // The building footprint itself remains grounded but is not walkable.
	Grid.EntranceCells = { 0 };
	const TArray<FPlotWalkwayTarget> Targets = { MakePlotWalkwayTarget(7, 800.f, 400.f, 1200.f, 800.f) };
	const FPlotWalkwayResult Diagonal = FPlotWalkwayRules::Solve(Grid, Targets, TConstArrayView<FBox2D>());
	TestTrue(TEXT("A plot with a ground hole is valid"), Diagonal.bValid);
	TestFalse(TEXT("A diagonal jump across missing ground cannot reach the building"),
		Diagonal.ReachableTargetIds.Contains(7));

	Grid.Ground[1] = 1;
	const FPlotWalkwayResult AroundCorner = FPlotWalkwayRules::Solve(Grid, Targets, TConstArrayView<FBox2D>());
	TestTrue(TEXT("A cardinal L route reaches the building"), AroundCorner.ReachableTargetIds.Contains(7));
	TestTrue(TEXT("The corner cell is used"), AroundCorner.PathCells.Contains(1));
	for (const int32 Cell : AroundCorner.PathCells)
	{
		TestTrue(TEXT("Every rendered cell lies on the authoritative ground mask"),
			Grid.Ground.IsValidIndex(Cell) && Grid.Ground[Cell] == 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRPlotWalkwayEmptyAndOrderTest,
	"CGR.CityWalkways.Rules.EmptyAndInputOrder",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRPlotWalkwayEmptyAndOrderTest::RunTest(const FString& Parameters)
{
	FPlotWalkwayGrid Grid = MakePlotWalkwayGrid(7, 5);
	Grid.EntranceCells = { 20, 14 };
	const FPlotWalkwayResult Empty = FPlotWalkwayRules::Solve(Grid,
		TConstArrayView<FPlotWalkwayTarget>(), TConstArrayView<FBox2D>());
	TestTrue(TEXT("Empty ground is a valid plot"), Empty.bValid);
	TestTrue(TEXT("No building means no decorative paving"), Empty.PathCells.IsEmpty());
	TestTrue(TEXT("No building means no reachable target"), Empty.ReachableTargetIds.IsEmpty());
	TestTrue(TEXT("No building selects no external entrance"), Empty.UsedEntranceCells.IsEmpty());
	FPlotWalkwayGrid NoEntrance = Grid;
	NoEntrance.EntranceCells.Reset();
	const TArray<FPlotWalkwayTarget> LonelyTarget = {
		MakePlotWalkwayTarget(1, 2000.f, 800.f, 2400.f, 1200.f)
	};
	const FPlotWalkwayResult Isolated = FPlotWalkwayRules::Solve(NoEntrance, LonelyTarget,
		TConstArrayView<FBox2D>());
	TestTrue(TEXT("Valid ground without an external entrance remains valid"), Isolated.bValid);
	TestTrue(TEXT("No external entrance yields no reachable building"),
		Isolated.ReachableTargetIds.IsEmpty());
	TestTrue(TEXT("No external entrance emits no path"), Isolated.PathCells.IsEmpty());

	const TArray<FPlotWalkwayTarget> ForwardTargets = {
		MakePlotWalkwayTarget(3, 2000.f, 800.f, 2400.f, 1200.f),
		MakePlotWalkwayTarget(2, 1200.f, 1200.f, 1600.f, 1600.f)
	};
	const TArray<FPlotWalkwayTarget> ReverseTargets = { ForwardTargets[1], ForwardTargets[0] };
	const TArray<FBox2D> ForwardBlockers = {
		MakePlotWalkwayBox(400.f, 0.f, 800.f, 400.f),
		MakePlotWalkwayBox(2400.f, 1600.f, 2800.f, 2000.f)
	};
	const TArray<FBox2D> ReverseBlockers = { ForwardBlockers[1], ForwardBlockers[0] };
	const FPlotWalkwayResult Forward = FPlotWalkwayRules::Solve(Grid, ForwardTargets, ForwardBlockers);
	Grid.EntranceCells = { 14, 20 };
	const FPlotWalkwayResult Reverse = FPlotWalkwayRules::Solve(Grid, ReverseTargets, ReverseBlockers);
	TestTrue(TEXT("Both permutations are valid"), Forward.bValid && Reverse.bValid);
	TestTrue(TEXT("Permutation preserves exact sorted paving cells"), Forward.PathCells == Reverse.PathCells);
	TestTrue(TEXT("Permutation preserves target reachability"),
		Forward.ReachableTargetIds.Num() == Reverse.ReachableTargetIds.Num()
		&& Forward.ReachableTargetIds.Contains(2) == Reverse.ReachableTargetIds.Contains(2)
		&& Forward.ReachableTargetIds.Contains(3) == Reverse.ReachableTargetIds.Contains(3));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRPlotWalkwayValidationTest,
	"CGR.CityWalkways.Rules.InvalidInputsAndLimits",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRPlotWalkwayValidationTest::RunTest(const FString& Parameters)
{
	FPlotWalkwayGrid Grid = MakePlotWalkwayGrid(2, 2);
	Grid.EntranceCells = { 0 };
	const TArray<FPlotWalkwayTarget> OneTarget = { MakePlotWalkwayTarget(1, 400.f, 400.f, 800.f, 800.f) };
	TestTrue(TEXT("A normal grid passes the validation baseline"),
		FPlotWalkwayRules::Solve(Grid, OneTarget, TConstArrayView<FBox2D>()).bValid);

	FPlotWalkwayGrid BadGrid = Grid;
	BadGrid.Size.X = 0;
	TestFalse(TEXT("Zero width is invalid"),
		FPlotWalkwayRules::Solve(BadGrid, OneTarget, TConstArrayView<FBox2D>()).bValid);
	BadGrid = Grid;
	BadGrid.Ground.RemoveAt(0);
	TestFalse(TEXT("Ground mask length must match the grid"),
		FPlotWalkwayRules::Solve(BadGrid, OneTarget, TConstArrayView<FBox2D>()).bValid);
	BadGrid = Grid;
	BadGrid.CellSize = 0.f;
	TestFalse(TEXT("Nonpositive cell width is invalid"),
		FPlotWalkwayRules::Solve(BadGrid, OneTarget, TConstArrayView<FBox2D>()).bValid);
	BadGrid = Grid;
	BadGrid.Origin.X = std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("NaN world origin is invalid"),
		FPlotWalkwayRules::Solve(BadGrid, OneTarget, TConstArrayView<FBox2D>()).bValid);
	BadGrid = Grid;
	BadGrid.EntranceCells = { 4 };
	TestFalse(TEXT("An out-of-range entrance is invalid"),
		FPlotWalkwayRules::Solve(BadGrid, OneTarget, TConstArrayView<FBox2D>()).bValid);
	BadGrid = Grid;
	BadGrid.EntranceDepth.Add(0, std::numeric_limits<double>::quiet_NaN());
	TestFalse(TEXT("Nonfinite entrance depth is invalid"),
		FPlotWalkwayRules::Solve(BadGrid, OneTarget, TConstArrayView<FBox2D>()).bValid);
	const TArray<FPlotWalkwayTarget> DuplicateIds = { OneTarget[0], OneTarget[0] };
	TestFalse(TEXT("Duplicate target IDs are invalid"),
		FPlotWalkwayRules::Solve(Grid, DuplicateIds, TConstArrayView<FBox2D>()).bValid);
	const TArray<FBox2D> BadBox = { MakePlotWalkwayBox(500.f, 0.f, 400.f, 100.f) };
	TestFalse(TEXT("Inverted blocker bounds are invalid"),
		FPlotWalkwayRules::Solve(Grid, OneTarget, BadBox).bValid);
	const TArray<FPlotWalkwayTarget> FlatTarget = { MakePlotWalkwayTarget(4, 0.f, 0.f, 0.f, 400.f) };
	TestFalse(TEXT("Zero-area target bounds are invalid"),
		FPlotWalkwayRules::Solve(Grid, FlatTarget, TConstArrayView<FBox2D>()).bValid);
	BadGrid = MakePlotWalkwayGrid(8193, 1);
	TestFalse(TEXT("More than 8192 cells are rejected"),
		FPlotWalkwayRules::Solve(BadGrid, TConstArrayView<FPlotWalkwayTarget>(),
			TConstArrayView<FBox2D>()).bValid);
	BadGrid = MakePlotWalkwayGrid(8192, 1);
	TestTrue(TEXT("The 8192-cell boundary itself is accepted"),
		FPlotWalkwayRules::Solve(BadGrid, TConstArrayView<FPlotWalkwayTarget>(),
			TConstArrayView<FBox2D>()).bValid);
	TArray<FPlotWalkwayTarget> TooManyTargets;
	for (int32 Id = 0; Id < 129; ++Id)
	{
		TooManyTargets.Add(MakePlotWalkwayTarget(Id, 0.f, 0.f, 100.f, 100.f));
	}
	TestFalse(TEXT("More than 128 targets are rejected"),
		FPlotWalkwayRules::Solve(Grid, TooManyTargets, TConstArrayView<FBox2D>()).bValid);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRPlotWalkwayActualCollisionTest,
	"CGR.CityWalkways.Rules.ActualCollisionCorridor",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRPlotWalkwayActualCollisionTest::RunTest(const FString& Parameters)
{
	FPlotWalkwayGrid Grid = MakePlotWalkwayGrid(5, 3);
	Grid.EntranceCells = { 5 };
	const TArray<FPlotWalkwayTarget> Targets = {
		MakePlotWalkwayTarget(1, 400.f, 0.f, 1200.f, 1200.f),
		MakePlotWalkwayTarget(2, 1600.f, 400.f, 2000.f, 800.f)
	};
	const FPlotWalkwayResult BroadBounds = FPlotWalkwayRules::Solve(Grid, Targets, TConstArrayView<FBox2D>());
	TestFalse(TEXT("Broad target bounds close the old route"), BroadBounds.ReachableTargetIds.Contains(2));
	const FBox2D ActualObstacle = MakePlotWalkwayBox(400.f, 0.f, 1200.f, 300.f);
	const FBox2D ActualBuilding = MakePlotWalkwayBox(1600.f, 400.f, 2000.f, 800.f);
	const auto ActualCollision = [ActualObstacle, ActualBuilding](const FBox2D& Bounds)
	{
		return OverlapsPlotWalkwayInterior(Bounds, ActualObstacle)
			|| OverlapsPlotWalkwayInterior(Bounds, ActualBuilding);
	};
	const FPlotWalkwayResult Open = FPlotWalkwayRules::Solve(
		Grid, Targets, TConstArrayView<FBox2D>(), true, ActualCollision);
	TestTrue(TEXT("Actual collision leaves a route inside a broad target box"),
		Open.ReachableTargetIds.Contains(2));
	TestTrue(TEXT("Route uses the clear cell inside that box"), Open.PathCells.Contains(6));
	TestFalse(TEXT("The actual occupied cell stays closed"), Open.PathCells.Contains(1));
	const FBox2D FullWall = MakePlotWalkwayBox(400.f, 0.f, 800.f, 1200.f);
	const FPlotWalkwayResult Closed = FPlotWalkwayRules::Solve(Grid, Targets,
		TConstArrayView<FBox2D>(), true, [FullWall](const FBox2D& Bounds)
		{
			return OverlapsPlotWalkwayInterior(Bounds, FullWall);
		});
	TestFalse(TEXT("Actual collision across the corridor blocks access"),
		Closed.ReachableTargetIds.Contains(2));

	FPlotWalkwayGrid ConnectorGrid = MakePlotWalkwayGrid(4, 3);
	ConnectorGrid.Ground.Init(0, 12);
	for (int32 X = 0; X < 4; ++X) { ConnectorGrid.Ground[4 + X] = 1; }
	ConnectorGrid.EntranceCells = { 4 };
	const TArray<FPlotWalkwayTarget> Recessed = {
		MakePlotWalkwayTarget(3, 1300.f, 400.f, 1700.f, 800.f)
	};
	const FBox2D GapObstacle = MakePlotWalkwayBox(1230.f, 400.f, 1270.f, 800.f);
	const FPlotWalkwayResult BlockedConnector = FPlotWalkwayRules::Solve(ConnectorGrid,
		Recessed, TConstArrayView<FBox2D>(), true, [GapObstacle](const FBox2D& Bounds)
		{
			return OverlapsPlotWalkwayInterior(Bounds, GapObstacle);
		});
	TestFalse(TEXT("Actual collision in a positive-gap connector blocks contact"),
		BlockedConnector.ReachableTargetIds.Contains(3));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRPlotWalkwayTrimRoadTest,
	"CGR.CityWalkways.Rules.TrimLongRoad",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRPlotWalkwayTrimRoadTest::RunTest(const FString& Parameters)
{
	const FBox2D Road = MakePlotWalkwayBox(0.f, 0.f, 10000.f, 1000.f);
	const FBox2D Obstacle = MakePlotWalkwayBox(3700.f, 0.f, 3900.f, 1000.f);
	const auto Collision = [Obstacle](const FBox2D& Bounds)
	{
		return OverlapsPlotWalkwayInterior(Bounds, Obstacle);
	};
	const FBox2D Trimmed = FPlotWalkwayRules::TrimStraightRoad(Road, FVector2D(1.f, 0.f), Collision);
	TestTrue(TEXT("Road longer than four cells remains valid"), Trimmed.bIsValid);
	TestEqual(TEXT("Trim keeps the starting face"), Trimmed.Min.X, Road.Min.X);
	TestEqual(TEXT("Trim keeps full road width"), Trimmed.Max.Y - Trimmed.Min.Y, 1000.0);
	TestTrue(TEXT("End stops within one centimetre of collision with a safety inset"),
		Trimmed.Max.X >= 3699.0 && Trimmed.Max.X < 3700.0);
	TestFalse(TEXT("The trimmed road is clear"), Collision(Trimmed));
	const FBox2D Clear = FPlotWalkwayRules::TrimStraightRoad(Road, FVector2D(1.f, 0.f),
		[](const FBox2D&) { return false; });
	TestEqual(TEXT("A clear road keeps its full grounded length"), Clear.Max.X, Road.Max.X);
	const FBox2D BlockedStart = FPlotWalkwayRules::TrimStraightRoad(Road, FVector2D(1.f, 0.f),
		[](const FBox2D& Bounds) { return Bounds.Min.X < 100.f; });
	TestFalse(TEXT("A blocked entrance cannot start a road"), BlockedStart.bIsValid);
	TestFalse(TEXT("Diagonal travel is invalid"),
		FPlotWalkwayRules::TrimStraightRoad(Road, FVector2D(1.f, 1.f), Collision).bIsValid);
	const FBox2D Reverse = FPlotWalkwayRules::TrimStraightRoad(Road, FVector2D(-1.f, 0.f), Collision);
	TestTrue(TEXT("Reverse travel trims the near collision face"),
		Reverse.bIsValid && Reverse.Min.X > 3900.0 && Reverse.Min.X <= 3901.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRPlotWalkwayEntranceDepthTest,
	"CGR.CityWalkways.Rules.EntranceDepthPriority",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRPlotWalkwayEntranceDepthTest::RunTest(const FString& Parameters)
{
	FPlotWalkwayGrid Grid = MakePlotWalkwayGrid(7, 5);
	Grid.EntranceCells = { 14, 20 };
	const TArray<FPlotWalkwayTarget> Targets = {
		MakePlotWalkwayTarget(1, 800.f, 800.f, 1200.f, 1200.f)
	};
	const FPlotWalkwayResult Nearest = FPlotWalkwayRules::Solve(Grid, Targets, TConstArrayView<FBox2D>());
	TestTrue(TEXT("Without depth the nearest entrance wins"),
		Nearest.UsedEntranceCells.Num() == 1 && Nearest.UsedEntranceCells[0] == 14);
	Grid.EntranceDepth.Add(14, 100.0);
	Grid.EntranceDepth.Add(20, 1500.0);
	const FPlotWalkwayResult Deepest = FPlotWalkwayRules::Solve(Grid, Targets, TConstArrayView<FBox2D>());
	TestTrue(TEXT("Depth takes precedence over mean target distance"),
		Deepest.UsedEntranceCells.Num() == 1 && Deepest.UsedEntranceCells[0] == 20);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRPlotRoadNetworkStraightAndBranchesTest,
	"CGR.CityWalkways.Rules.RoadNetworkStraightAndBranches",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRPlotRoadNetworkStraightAndBranchesTest::RunTest(const FString& Parameters)
{
	const auto Clear = [](const FBox2D&) { return true; };
	const FPlotRoadNetwork Straight = FPlotWalkwayRules::BuildRoadNetwork(
		MakePlotWalkwayBox(-500.f, -500.f, 5500.f, 500.f), FVector2D::ZeroVector,
		FVector2D(1.f, 0.f), 1000.0, 500.0, Clear);
	TestEqual(TEXT("A deep straight corridor has eleven reachable nodes"), Straight.ReachableNodeCount, 11);
	if (TestEqual(TEXT("A deep straight corridor merges to one road segment"), Straight.Segments.Num(), 1))
	{
		TestEqual(TEXT("The segment keeps the complete grounded length"), Straight.Segments[0].Max.X, 5500.0);
		TestEqual(TEXT("The segment keeps 1000 cm width"),
			Straight.Segments[0].Max.Y - Straight.Segments[0].Min.Y, 1000.0);
	}
	TestEqual(TEXT("Straight route has no turn square"), Straight.Junctions.Num(), 0);
	TestEqual(TEXT("Every segment has one direction"), Straight.Segments.Num(), Straight.Directions.Num());

	const FPlotRoadNetwork Branches = FPlotWalkwayRules::BuildRoadNetwork(
		MakePlotWalkwayBox(-500.f, -2500.f, 3500.f, 2500.f), FVector2D::ZeroVector,
		FVector2D(1.f, 0.f), 1000.0, 500.0, Clear);
	bool bLeft = false;
	bool bRight = false;
	double RoadAreaUpperBound = 0.0;
	for (const FBox2D& Segment : Branches.Segments)
	{
		RoadAreaUpperBound += Segment.GetSize().X * Segment.GetSize().Y;
		bLeft |= Segment.Max.Y >= 2500.0;
		bRight |= Segment.Min.Y <= -2500.0;
		TestTrue(TEXT("All branches are full road width or a wider junction overlap"),
			Segment.Max.X - Segment.Min.X >= 1000.0 && Segment.Max.Y - Segment.Min.Y >= 1000.0);
	}
	TestTrue(TEXT("Sparse road reaches both lateral extrema"), bLeft && bRight);
	TestTrue(TEXT("Route union does not pave every reachable square"),
		Branches.ReachableNodeCount > 20 && Branches.Segments.Num() < Branches.ReachableNodeCount / 2);
	TestTrue(TEXT("The open plot retains at least thirty percent unpaved space"),
		RoadAreaUpperBound <= 4000.0 * 5000.0 * 0.7);
	const FPlotRoadNetwork Repeat = FPlotWalkwayRules::BuildRoadNetwork(
		MakePlotWalkwayBox(-500.f, -2500.f, 3500.f, 2500.f), FVector2D::ZeroVector,
		FVector2D(1.f, 0.f), 1000.0, 500.0, Clear);
	bool bSame = Branches.ReachableNodeCount == Repeat.ReachableNodeCount
		&& Branches.Segments.Num() == Repeat.Segments.Num()
		&& Branches.Directions == Repeat.Directions
		&& Branches.Junctions.Num() == Repeat.Junctions.Num();
	for (int32 Index = 0; bSame && Index < Branches.Segments.Num(); ++Index)
	{
		bSame = Branches.Segments[Index].Min == Repeat.Segments[Index].Min
			&& Branches.Segments[Index].Max == Repeat.Segments[Index].Max;
	}
	for (int32 Index = 0; bSame && Index < Branches.Junctions.Num(); ++Index)
	{
		bSame = Branches.Junctions[Index].Min == Repeat.Junctions[Index].Min
			&& Branches.Junctions[Index].Max == Repeat.Junctions[Index].Max;
	}
	TestTrue(TEXT("Identical inputs produce identical ordered network geometry"), bSame);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRPlotRoadNetworkCollisionTest,
	"CGR.CityWalkways.Rules.RoadNetworkCollisionAndDetour",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRPlotRoadNetworkCollisionTest::RunTest(const FString& Parameters)
{
	const FBox2D Bounds = MakePlotWalkwayBox(-500.f, -1500.f, 5500.f, 1500.f);
	const FBox2D Block = MakePlotWalkwayBox(1500.f, -500.f, 2500.f, 500.f);
	const auto IsClear = [Block](const FBox2D& Candidate)
	{
		return !OverlapsPlotWalkwayInterior(Candidate, Block);
	};
	const FPlotRoadNetwork Detour = FPlotWalkwayRules::BuildRoadNetwork(
		Bounds, FVector2D::ZeroVector, FVector2D(1.f, 0.f), 1000.0, 500.0, IsClear);
	bool bPassedBlock = false;
	bool bLateral = false;
	for (const FBox2D& Segment : Detour.Segments)
	{
		bPassedBlock |= Segment.Max.X >= 5000.0;
		bLateral |= Segment.Min.Y <= -1000.0 || Segment.Max.Y >= 1000.0;
		TestTrue(TEXT("Every merged road segment is clear"), IsClear(Segment));
	}
	for (const FBox2D& Junction : Detour.Junctions)
	{
		TestTrue(TEXT("Every turn and branch square is clear"), IsClear(Junction));
	}
	TestTrue(TEXT("A blocked straight lane detours laterally and progresses forward"),
		bPassedBlock && bLateral);
	TestTrue(TEXT("The blocked area itself is never paved"),
		!Detour.Segments.ContainsByPredicate([Block](const FBox2D& Segment)
		{
			return OverlapsPlotWalkwayInterior(Segment, Block);
		}));
	const FBox2D RootTurnBlock = MakePlotWalkwayBox(500.f, -500.f, 1500.f, 500.f);
	const FPlotRoadNetwork RootTurn = FPlotWalkwayRules::BuildRoadNetwork(Bounds,
		FVector2D::ZeroVector, FVector2D(1.f, 0.f), 1000.0, 500.0,
		[RootTurnBlock](const FBox2D& Candidate)
		{
			return !OverlapsPlotWalkwayInterior(Candidate, RootTurnBlock);
		});
	TestTrue(TEXT("A first lateral edge exports the entrance square as a junction"),
		RootTurn.Junctions.ContainsByPredicate([](const FBox2D& Box)
		{
			return Box.Min == FVector2D(-500.f, -500.f) && Box.Max == FVector2D(500.f, 500.f);
		}));
	const FPlotRoadNetwork Narrow = FPlotWalkwayRules::BuildRoadNetwork(
		MakePlotWalkwayBox(-500.f, -500.f, 3500.f, 500.f), FVector2D::ZeroVector,
		FVector2D(1.f, 0.f), 1000.0, 500.0,
		[](const FBox2D& Candidate)
		{
			return Candidate.Max.X <= 1000.0
				|| (Candidate.Min.Y >= -450.0 && Candidate.Max.Y <= 450.0);
		});
	TestTrue(TEXT("A 900 cm bottleneck cannot carry a 1000 cm road"),
		Narrow.ReachableNodeCount <= 2 && Narrow.Segments.Num() <= 1);
	const FPlotRoadNetwork BlockedStart = FPlotWalkwayRules::BuildRoadNetwork(
		Bounds, FVector2D::ZeroVector, FVector2D(1.f, 0.f), 1000.0, 500.0,
		[](const FBox2D&) { return false; });
	TestTrue(TEXT("Blocked start produces an empty network"),
		BlockedStart.ReachableNodeCount == 0 && BlockedStart.Segments.IsEmpty()
		&& BlockedStart.Junctions.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRPlotRoadNetworkDeadEndTest,
	"CGR.CityWalkways.Rules.RoadNetworkDeadEndPruning",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRPlotRoadNetworkDeadEndTest::RunTest(const FString& Parameters)
{
	const auto Clear = [](const FBox2D&) { return true; };
	FPlotRoadNetworkOptions Options;
	Options.MinimumDeadEndLength = 2000.0;
	const FPlotRoadNetwork TooShort = FPlotWalkwayRules::BuildRoadNetwork(
		MakePlotWalkwayBox(-500.f, -500.f, 2000.f, 500.f), FVector2D::ZeroVector,
		FVector2D(1.f, 0.f), 1000.0, 500.0, Clear, Options);
	TestTrue(TEXT("A complete dead-end shorter than 2000 cm is removed"),
		TooShort.Segments.IsEmpty() && TooShort.Junctions.IsEmpty());
	Options.InitialLength = 750.0;
	const FPlotRoadNetwork Supported = FPlotWalkwayRules::BuildRoadNetwork(
		MakePlotWalkwayBox(-500.f, -500.f, 2000.f, 500.f), FVector2D::ZeroVector,
		FVector2D(1.f, 0.f), 1000.0, 500.0, Clear, Options);
	TestFalse(TEXT("The entrance's initial length contributes to the main dead-end"),
		Supported.Segments.IsEmpty());
	Options.InitialLength = 0.0;
	const FPlotRoadNetwork ShortBranch = FPlotWalkwayRules::BuildRoadNetwork(
		MakePlotWalkwayBox(-500.f, -500.f, 3500.f, 1500.f), FVector2D::ZeroVector,
		FVector2D(1.f, 0.f), 1000.0, 500.0, Clear, Options);
	TestTrue(TEXT("A long main road remains after pruning a short branch"),
		!ShortBranch.Segments.IsEmpty());
	TestFalse(TEXT("The side branch shorter than 2000 cm is removed"),
		ShortBranch.Segments.ContainsByPredicate([](const FBox2D& Segment)
		{
			return Segment.Max.Y > 500.0;
		}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRPlotRoadNetworkConnectionTest,
	"CGR.CityWalkways.Rules.RoadNetworkExternalConnection",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRPlotRoadNetworkConnectionTest::RunTest(const FString& Parameters)
{
	FPlotRoadNetworkOptions Options;
	Options.MinimumDeadEndLength = 2000.0;
	Options.FindConnection = [](const FVector2D& Center, const FVector2D& Direction,
		FPlotRoadConnection& Out) -> bool
	{
		if (Center == FVector2D(3000.f, 1000.f) && Direction == FVector2D(0.f, 1.f))
		{
			Out.Bounds = MakePlotWalkwayBox(2500.f, 1000.f, 3500.f, 2000.f);
			Out.JunctionBounds = MakePlotWalkwayBox(2500.f, 500.f, 3500.f, 1500.f);
			Out.Direction = Direction;
			return true;
		}
		return false;
	};
	const FPlotRoadNetwork Connected = FPlotWalkwayRules::BuildRoadNetwork(
		MakePlotWalkwayBox(-500.f, -500.f, 3500.f, 1500.f), FVector2D::ZeroVector,
		FVector2D(1.f, 0.f), 1000.0, 500.0,
		[](const FBox2D&) { return true; }, Options);
	TestEqual(TEXT("The short branch to an external road is retained"), Connected.Connections.Num(), 1);
	TestTrue(TEXT("Its lateral connector remains attached to the selected route"),
		Connected.Segments.ContainsByPredicate([](const FBox2D& Segment)
		{
			return Segment.Max.Y > 500.0;
		}));
	if (Connected.Connections.Num() == 1)
	{
		TestEqual(TEXT("Connection direction is preserved"),
			Connected.Connections[0].Direction, FVector2D(0.f, 1.f));
	}

	Options.MinimumDeadEndLength = 0.0;
	Options.FindConnection = [](const FVector2D& Center, const FVector2D& Direction,
		FPlotRoadConnection& Out) -> bool
	{
		if (Center == FVector2D(1500.f, 0.f) && Direction == FVector2D(0.f, 1.f))
		{
			Out.Bounds = MakePlotWalkwayBox(1000.f, 0.f, 2000.f, 1000.f);
			Out.JunctionBounds = MakePlotWalkwayBox(1000.f, -500.f, 2000.f, 500.f);
			Out.Direction = Direction;
			return true;
		}
		return false;
	};
	const FPlotRoadNetwork Preferred = FPlotWalkwayRules::BuildRoadNetwork(
		MakePlotWalkwayBox(-500.f, -500.f, 4500.f, 1500.f), FVector2D::ZeroVector,
		FVector2D(1.f, 0.f), 1000.0, 500.0,
		[](const FBox2D&) { return true; }, Options);
	TestTrue(TEXT("A reachable external connection is selected before a farther geometric side extreme"),
		Preferred.Connections.Num() == 1
		&& Preferred.Connections[0].JunctionBounds.GetCenter() == FVector2D(1500.f, 0.f));
	Options.MinimumDeadEndLength = 2000.0;
	Options.FindConnection = [](const FVector2D& Center, const FVector2D& Direction,
		FPlotRoadConnection& Out) -> bool
	{
		if (Center != FVector2D::ZeroVector || Direction != FVector2D(0.f, 1.f)) { return false; }
		Out.Bounds = MakePlotWalkwayBox(-500.f, 0.f, 500.f, 1000.f);
		Out.JunctionBounds = MakePlotWalkwayBox(-500.f, 500.f, 500.f, 1500.f);
		Out.Direction = Direction;
		return true;
	};
	const FPlotRoadNetwork Immediate = FPlotWalkwayRules::BuildRoadNetwork(
		MakePlotWalkwayBox(-500.f, -500.f, 500.f, 500.f), FVector2D::ZeroVector,
		FVector2D(1.f, 0.f), 1000.0, 500.0,
		[](const FBox2D&) { return true; }, Options);
	TestTrue(TEXT("An immediate opposite-road connection survives with no internal grid edges"),
		Immediate.Connections.Num() == 1 && Immediate.Segments.IsEmpty()
		&& Immediate.Junctions.Num() == 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRPlotRoadNetworkEdgeFilterTest,
	"CGR.CityWalkways.Rules.RoadNetworkDirectionalEdgeFilter",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRPlotRoadNetworkEdgeFilterTest::RunTest(const FString& Parameters)
{
	FPlotRoadNetworkOptions Options;
	Options.IsEdgeAllowed = [](const FBox2D& Sweep, const FVector2D& Direction)
	{
		return Direction != FVector2D(1.f, 0.f) || Sweep.Max.X <= 1500.0;
	};
	const FPlotRoadNetwork Filtered = FPlotWalkwayRules::BuildRoadNetwork(
		MakePlotWalkwayBox(-500.f, -500.f, 3500.f, 500.f), FVector2D::ZeroVector,
		FVector2D(1.f, 0.f), 1000.0, 500.0,
		[](const FBox2D&) { return true; }, Options);
	TestTrue(TEXT("Directional edge filter stops an otherwise clear parallel road"),
		!Filtered.Segments.IsEmpty() && Filtered.Segments.Last().Max.X <= 1500.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRPlotRoadNetworkParallelTest,
	"CGR.CityWalkways.Rules.RoadNetworkParallelGoals",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRPlotRoadNetworkParallelTest::RunTest(const FString& Parameters)
{
	FPlotRoadNetworkOptions Options;
	Options.IsEdgeAllowed = [](const FBox2D& Sweep, const FVector2D& Direction)
	{
		return Direction != FVector2D(0.f, 1.f) || Sweep.GetCenter().X <= 500.0;
	};
	const FBox2D Bounds = MakePlotWalkwayBox(-500.f, -500.f, 3500.f, 1500.f);
	const auto Clear = [](const FBox2D&) { return true; };
	const FPlotRoadNetwork Original = FPlotWalkwayRules::BuildRoadNetwork(
		Bounds, FVector2D::ZeroVector, FVector2D(1.f, 0.f), 1000.0, 500.0,
		Clear, Options);
	TestTrue(TEXT("Without a spacing limit the lateral goal creates a parallel run"),
		Original.Segments.ContainsByPredicate([](const FBox2D& Segment)
		{
			return Segment.Max.Y > 500.0 && Segment.GetSize().X > 500.0;
		}));
	Options.MinimumParallelGap = 2000.0;
	const FPlotRoadNetwork Filtered = FPlotWalkwayRules::BuildRoadNetwork(
		Bounds, FVector2D::ZeroVector, FVector2D(1.f, 0.f), 1000.0, 500.0,
		Clear, Options);
	TestTrue(TEXT("The first through route remains without a nearby parallel branch"),
		Filtered.Segments.ContainsByPredicate([](const FBox2D& Segment)
		{
			return Segment.Max.X >= 3500.0 && Segment.Max.Y <= 500.0;
		})
		&& !Filtered.Segments.ContainsByPredicate([](const FBox2D& Segment)
		{
			return Segment.Max.Y > 500.0;
		}));
	return true;
}

#endif
