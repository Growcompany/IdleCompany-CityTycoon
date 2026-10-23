#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Entity/Plot/CityPlotActor.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
bool HasBoundarySegment(const TArray<FCityPlotBoundarySegment>& Segments,
	const FVector2D& Start, const FVector2D& End, const FVector2D& InwardNormal)
{
	for (const FCityPlotBoundarySegment& Segment : Segments)
	{
		if (Segment.Start.Equals(Start, 0.01) && Segment.End.Equals(End, 0.01)
			&& Segment.InwardNormal.Equals(InwardNormal, 0.01))
		{
			return true;
		}
	}
	return false;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGRDressingPlotBoundaryTest,
	"CGR.CityDressing.AuthoredPlotBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCGRDressingPlotBoundaryTest::RunTest(const FString& Parameters)
{
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::EditorPreview, false);
	if (!TestNotNull(TEXT("isolated plot world"), TestWorld)) { return false; }
	if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::EditorPreview).SetCurrentWorld(TestWorld); }
	ACityPlotActor* Plot = TestWorld->SpawnActor<ACityPlotActor>();
	const FStructProperty* GridProperty = FindFProperty<FStructProperty>(ACityPlotActor::StaticClass(), TEXT("PlotSizeCells"));
	const FArrayProperty* ExcludedProperty = FindFProperty<FArrayProperty>(ACityPlotActor::StaticClass(), TEXT("ExcludedCells"));
	if (TestTrue(TEXT("plot fixture and authored properties"), Plot && GridProperty && ExcludedProperty))
	{
		*GridProperty->ContainerPtrToValuePtr<FIntPoint>(Plot) = FIntPoint(3, 2);
		Plot->InitFromAuthoredBounds();
		const FVector2D Origin = FVector2D(Plot->GetActorLocation().X, Plot->GetActorLocation().Y) - Plot->GetExtent();
		const double Cell = Plot->GetExtent().X * 2.0 / 3.0;
		const auto Point = [&Origin, Cell](double X, double Y)
		{
			return Origin + FVector2D(X * Cell, Y * Cell);
		};

		TArray<FCityPlotBoundarySegment> Segments;
		Plot->GetBoundarySegments(Segments);
		TestEqual(TEXT("rectangle has four merged outer runs"), Segments.Num(), 4);
		TestTrue(TEXT("rectangle bottom points inward"), HasBoundarySegment(Segments, Point(0, 0), Point(3, 0), FVector2D(0, 1)));
		TestTrue(TEXT("rectangle top points inward"), HasBoundarySegment(Segments, Point(3, 2), Point(0, 2), FVector2D(0, -1)));
		TestTrue(TEXT("rectangle left points inward"), HasBoundarySegment(Segments, Point(0, 2), Point(0, 0), FVector2D(1, 0)));
		TestTrue(TEXT("rectangle right points inward"), HasBoundarySegment(Segments, Point(3, 0), Point(3, 2), FVector2D(-1, 0)));

		ExcludedProperty->ContainerPtrToValuePtr<TArray<FIntPoint>>(Plot)->Add(FIntPoint(2, 1));
		Plot->GetBoundarySegments(Segments);
		TestEqual(TEXT("notched L has six runs"), Segments.Num(), 6);
		TestTrue(TEXT("L bottom stays full width"), HasBoundarySegment(Segments, Point(0, 0), Point(3, 0), FVector2D(0, 1)));
		TestTrue(TEXT("L left stays full height"), HasBoundarySegment(Segments, Point(0, 2), Point(0, 0), FVector2D(1, 0)));
		TestTrue(TEXT("L top stops at notch"), HasBoundarySegment(Segments, Point(2, 2), Point(0, 2), FVector2D(0, -1)));
		TestTrue(TEXT("L outer right ends at notch"), HasBoundarySegment(Segments, Point(3, 0), Point(3, 1), FVector2D(-1, 0)));
		TestTrue(TEXT("notch floor faces valid lower cell"), HasBoundarySegment(Segments, Point(3, 1), Point(2, 1), FVector2D(0, -1)));
		TestTrue(TEXT("notch wall faces valid left cell"), HasBoundarySegment(Segments, Point(2, 1), Point(2, 2), FVector2D(-1, 0)));
		TestFalse(TEXT("shared internal cell edge is absent"), HasBoundarySegment(Segments, Point(1, 0), Point(1, 2), FVector2D(-1, 0)));
		TestFalse(TEXT("no diagonal cuts across notch"), HasBoundarySegment(Segments, Point(3, 1), Point(2, 2), FVector2D(-1, -1)));
	}
	if (GEngine) { GEngine->DestroyWorldContext(TestWorld); }
	TestWorld->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGRDressingPhysicalBoundaryTest,
	"CGR.CityDressing.PhysicalPlotBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCGRDressingPhysicalBoundaryTest::RunTest(const FString& Parameters)
{
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::EditorPreview, false);
	if (!TestNotNull(TEXT("isolated plot world"), TestWorld)) { return false; }
	if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::EditorPreview).SetCurrentWorld(TestWorld); }
	ACityPlotActor* Plot = TestWorld->SpawnActor<ACityPlotActor>();
	const FStructProperty* GridProperty = FindFProperty<FStructProperty>(ACityPlotActor::StaticClass(), TEXT("PlotSizeCells"));
	const FArrayProperty* BoundaryProperty = FindFProperty<FArrayProperty>(ACityPlotActor::StaticClass(), TEXT("DressingBoundaryLocalVertices"));
	if (TestTrue(TEXT("plot fixture and dressing override"), Plot && GridProperty && BoundaryProperty))
	{
		*GridProperty->ContainerPtrToValuePtr<FIntPoint>(Plot) = FIntPoint(4, 4);
		Plot->InitFromAuthoredBounds();
		const double Cell = Plot->GetExtent().X * 0.5;
		auto* Vertices = BoundaryProperty->ContainerPtrToValuePtr<TArray<FVector2D>>(Plot);
		*Vertices = TArray<FVector2D>{
			FVector2D(-2 * Cell, -2 * Cell), FVector2D(2 * Cell, -2 * Cell),
			FVector2D(2 * Cell, 0), FVector2D(0, 0),
			FVector2D(0, 2 * Cell), FVector2D(-2 * Cell, 2 * Cell)
		};
		TArray<FCityPlotBoundarySegment> Segments;
		TestTrue(TEXT("physical L boundary is valid"), Plot->GetDressingBoundarySegments(Segments));
		TestEqual(TEXT("physical L has six runs"), Segments.Num(), 6);
		TestTrue(TEXT("physical L concave floor points into valid land"),
			HasBoundarySegment(Segments, FVector2D(2 * Cell, 0), FVector2D(0, 0), FVector2D(0, -1)));
		TestTrue(TEXT("physical L concave wall points into valid land"),
			HasBoundarySegment(Segments, FVector2D(0, 0), FVector2D(0, 2 * Cell), FVector2D(-1, 0)));
		TestFalse(TEXT("logical rectangle cannot permit dressing in physical L notch"),
			Plot->IsDressingFootprintAllowed(FVector2D(Cell, Cell), FVector2D(Cell * 0.2, Cell * 0.2)));

		// Top indentation forms a U: all four corners of the large candidate are inside,
		// but its middle still crosses the open notch.
		*Vertices = TArray<FVector2D>{
			FVector2D(-2 * Cell, -2 * Cell), FVector2D(2 * Cell, -2 * Cell),
			FVector2D(2 * Cell, 2 * Cell), FVector2D(Cell * 0.5, 2 * Cell),
			FVector2D(Cell * 0.5, 0), FVector2D(-Cell * 0.5, 0),
			FVector2D(-Cell * 0.5, 2 * Cell), FVector2D(-2 * Cell, 2 * Cell)
		};
		TestTrue(TEXT("valid physical boundary is available"), Plot->GetDressingBoundarySegments(Segments));
		TestEqual(TEXT("physical U has eight edges"), Segments.Num(), 8);
		TestTrue(TEXT("notch walls point into usable sidewalk"),
			HasBoundarySegment(Segments, FVector2D(Cell * 0.5, 2 * Cell), FVector2D(Cell * 0.5, 0), FVector2D(1, 0)));
		TestTrue(TEXT("small patch on left arm is allowed"),
			Plot->IsDressingFootprintAllowed(FVector2D(-Cell, Cell), FVector2D(Cell * 0.2, Cell * 0.2)));
		TestFalse(TEXT("four valid corners cannot hide a notch crossing"),
			Plot->IsDressingFootprintAllowed(FVector2D(0, -Cell * 0.25), FVector2D(Cell, Cell)));
		TestFalse(TEXT("patch within logical cells but inside physical notch is rejected"),
			Plot->IsDressingFootprintAllowed(FVector2D(0, Cell), FVector2D(Cell * 0.2, Cell * 0.2)));

		*Vertices = TArray<FVector2D>{ FVector2D(-Cell, -Cell), FVector2D(Cell, -Cell), FVector2D(0, Cell) };
		TestFalse(TEXT("invalid nonempty polygon fails closed"), Plot->GetDressingBoundarySegments(Segments));
		TestEqual(TEXT("invalid polygon exposes no edges"), Segments.Num(), 0);
		TestFalse(TEXT("invalid polygon permits no dressing"),
			Plot->IsDressingFootprintAllowed(FVector2D(0, 0), FVector2D(Cell * 0.1, Cell * 0.1)));

		Vertices->Reset();
		TestTrue(TEXT("empty override restores logical boundary"), Plot->GetDressingBoundarySegments(Segments));
		TestEqual(TEXT("logical rectangle has four edges"), Segments.Num(), 4);
		TestTrue(TEXT("logical rectangle again accepts center"),
			Plot->IsDressingFootprintAllowed(FVector2D(0, 0), FVector2D(Cell * 0.1, Cell * 0.1)));
	}
	if (GEngine) { GEngine->DestroyWorldContext(TestWorld); }
	TestWorld->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGRDressingExcludedCellTest,
	"CGR.CityDressing.AuthoredExcludedCells",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCGRDressingExcludedCellTest::RunTest(const FString& Parameters)
{
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::EditorPreview, false);
	if (!TestNotNull(TEXT("isolated plot world"), TestWorld)) { return false; }
	if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::EditorPreview).SetCurrentWorld(TestWorld); }
	ACityPlotActor* Plot = TestWorld->SpawnActor<ACityPlotActor>();
	const FStructProperty* GridProperty = FindFProperty<FStructProperty>(ACityPlotActor::StaticClass(), TEXT("PlotSizeCells"));
	const FArrayProperty* ExcludedProperty = FindFProperty<FArrayProperty>(ACityPlotActor::StaticClass(), TEXT("ExcludedCells"));
	if (TestTrue(TEXT("plot fixture and authored properties"), Plot && GridProperty && ExcludedProperty))
	{
		*GridProperty->ContainerPtrToValuePtr<FIntPoint>(Plot) = FIntPoint(3, 3);
		ExcludedProperty->ContainerPtrToValuePtr<TArray<FIntPoint>>(Plot)->Add(FIntPoint(1, 1));
		Plot->InitFromAuthoredBounds();
		const double Cell = Plot->GetExtent().X * 2.0 / 3.0;
		TestTrue(TEXT("a usable cell accepts a small patch"),
			Plot->IsDressingFootprintAllowed(FVector2D(-Cell, -Cell), FVector2D(Cell * 0.2, Cell * 0.2)));
		TestFalse(TEXT("excluded cell rejects a patch even without ground collision"),
			Plot->IsDressingFootprintAllowed(FVector2D::ZeroVector, FVector2D(Cell * 0.2, Cell * 0.2)));
		TestFalse(TEXT("a valid center cannot extend into the excluded cell"),
			Plot->IsDressingFootprintAllowed(FVector2D(-Cell * 0.55, 0.0), FVector2D(Cell * 0.3, Cell * 0.2)));
		TestTrue(TEXT("touching the excluded cell edge is allowed"),
			Plot->IsDressingFootprintAllowed(FVector2D(-Cell, 0.0), FVector2D(Cell * 0.5, Cell * 0.2)));
		TestFalse(TEXT("an off-plot patch is rejected"),
			Plot->IsDressingFootprintAllowed(FVector2D(-Cell * 1.4, 0.0), FVector2D(Cell * 0.2, Cell * 0.2)));
	}
	if (GEngine) { GEngine->DestroyWorldContext(TestWorld); }
	TestWorld->DestroyWorld(false);
	return true;
}

#endif
