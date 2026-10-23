#include "Misc/AutomationTest.h"
#include "Office/OfficeExteriorLayout.h"
#include "Office/OfficeFootprintGeometry.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
FBox2D GetCubeXYBounds(const FTransform& Transform)
{
	const FVector Location = Transform.GetLocation();
	const FVector Scale = Transform.GetScale3D();
	const FVector2D Center(Location.X, Location.Y);
	const FVector2D HalfSize(Scale.X * 50.f, Scale.Y * 50.f);
	return FBox2D(Center - HalfSize, Center + HalfSize);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfficeExteriorOwnBuildingLayoutTest,
	"CGR.Office.Exterior.OwnBuilding.FootprintAndGround",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FOfficeExteriorOwnBuildingLayoutTest::RunTest(const FString& Parameters)
{
	for (const FIntPoint TileCount : {FIntPoint(1, 2), FIntPoint(3, 4), FIntPoint(5, 6)})
	{
		FOfficeExteriorLayoutInput Input;
		Input.TileCount = TileCount;
		Input.MaxTileCount = FOfficeExteriorLayoutBuilder::GetCanonicalMaxTileCount();
		Input.CurrentFloorBounds = FOfficeFootprintGeometry::MakeFloorBounds(TileCount, 400.f);
		Input.MaxFloorBounds = FOfficeFootprintGeometry::MakeFloorBounds(Input.MaxTileCount, 400.f);
		Input.TileSizeCm = 400.f;
		Input.StructuralFloorZ = 125.f;
		Input.OfficeHeightCm = 14000.f;
		FOfficeExteriorLayoutResult Layout;
		if (!TestTrue(TEXT("own-building layout builds"), FOfficeExteriorLayoutBuilder::Build(Input, Layout)))
		{
			continue;
		}
		TestTrue(TEXT("exterior follows current floor bounds"),
			Layout.ExteriorFootprintBounds.Min.Equals(Input.CurrentFloorBounds.Min, 0.1)
			&& Layout.ExteriorFootprintBounds.Max.Equals(Input.CurrentFloorBounds.Max, 0.1));
		TestEqual(TEXT("ten facade floors follow the active tile count"),
			Layout.FacadeBayTransforms.Num(), 10 * (TileCount.X + TileCount.Y));
		TestEqual(TEXT("floor bands stay paired"), Layout.FloorBandTransforms.Num(), 20);
		TestEqual(TEXT("corner and trim count"), Layout.CornerAndTrimTransforms.Num(), 5);
		TestTrue(TEXT("dynamic exterior remains under the authored budget"),
			Layout.GetDynamicInstanceCount() <= 135);
		TestEqual(TEXT("ground tracks source building height"), Layout.GroundZ, Input.StructuralFloorZ - Input.OfficeHeightCm);
		TestTrue(TEXT("tower reaches the ground"), FMath::IsNearlyEqual(
			Layout.TowerBodyTransform.GetLocation().Z - Layout.TowerBodyTransform.GetScale3D().Z * 50.f,
			Layout.GroundZ, 0.1));
		TestTrue(TEXT("tower footprint tracks current office"),
			Layout.TowerTopBounds.Min.Equals(Input.CurrentFloorBounds.Min, 0.1)
			&& Layout.TowerTopBounds.Max.Equals(Input.CurrentFloorBounds.Max, 0.1));

		const int32 DefaultInstanceCount = Layout.GetDynamicInstanceCount();
		Input.ExteriorFootprintBounds = FBox2D(
			Input.CurrentFloorBounds.Min - FVector2D(20.f, 0.75f),
			Input.CurrentFloorBounds.Max + FVector2D(0.f, 20.f));
		if (!TestTrue(TEXT("asymmetric wall bounds build"), FOfficeExteriorLayoutBuilder::Build(Input, Layout)))
		{
			continue;
		}
		const FBox2D ExteriorBounds = Input.ExteriorFootprintBounds;
		TestTrue(TEXT("layout reports the wall bounds"),
			Layout.ExteriorFootprintBounds.Min.Equals(ExteriorBounds.Min, 0.1)
			&& Layout.ExteriorFootprintBounds.Max.Equals(ExteriorBounds.Max, 0.1));
		TestTrue(TEXT("slab uses wall bounds with its edge inset"),
			GetCubeXYBounds(Layout.ApronSlabTransform).Min.Equals(ExteriorBounds.Min + FVector2D(12.f, 12.f), 0.1)
			&& GetCubeXYBounds(Layout.ApronSlabTransform).Max.Equals(ExteriorBounds.Max - FVector2D(12.f, 12.f), 0.1));
		TestTrue(TEXT("front facade band reaches the outer wall face"),
			FMath::IsNearlyEqual(GetCubeXYBounds(Layout.FloorBandTransforms[0]).Min.Y, ExteriorBounds.Min.Y, 0.1));
		TestTrue(TEXT("right facade band reaches the outer wall face"),
			FMath::IsNearlyEqual(GetCubeXYBounds(Layout.FloorBandTransforms[1]).Max.X, ExteriorBounds.Max.X, 0.1));
		TestTrue(TEXT("corner posts reach the left, front and back wall faces"),
			FMath::IsNearlyEqual(GetCubeXYBounds(Layout.CornerAndTrimTransforms[0]).Min.X, ExteriorBounds.Min.X, 0.1)
			&& FMath::IsNearlyEqual(GetCubeXYBounds(Layout.CornerAndTrimTransforms[1]).Min.Y, ExteriorBounds.Min.Y, 0.1)
			&& FMath::IsNearlyEqual(GetCubeXYBounds(Layout.CornerAndTrimTransforms[2]).Max.Y, ExteriorBounds.Max.Y, 0.1));
		TestTrue(TEXT("top trim reaches the front and right wall faces"),
			FMath::IsNearlyEqual(GetCubeXYBounds(Layout.CornerAndTrimTransforms[3]).Min.Y, ExteriorBounds.Min.Y, 0.1)
			&& FMath::IsNearlyEqual(GetCubeXYBounds(Layout.CornerAndTrimTransforms[4]).Max.X, ExteriorBounds.Max.X, 0.1));
		TestTrue(TEXT("transfer and tower use wall bounds"),
			GetCubeXYBounds(Layout.TransferLevelTransform).Min.Equals(ExteriorBounds.Min, 0.1)
			&& GetCubeXYBounds(Layout.TransferLevelTransform).Max.Equals(ExteriorBounds.Max, 0.1)
			&& GetCubeXYBounds(Layout.TowerBodyTransform).Min.Equals(ExteriorBounds.Min, 0.1)
			&& GetCubeXYBounds(Layout.TowerBodyTransform).Max.Equals(ExteriorBounds.Max, 0.1)
			&& Layout.TowerTopBounds.Min.Equals(ExteriorBounds.Min, 0.1)
			&& Layout.TowerTopBounds.Max.Equals(ExteriorBounds.Max, 0.1));
		TestEqual(TEXT("wall thickness does not add facade instances"),
			Layout.GetDynamicInstanceCount(), DefaultInstanceCount);

		Input.ExteriorFootprintBounds.Min.X = Input.CurrentFloorBounds.Min.X + 1.f;
		TestFalse(TEXT("wall bounds inside the floor are rejected"),
			FOfficeExteriorLayoutBuilder::Build(Input, Layout));
		TestTrue(TEXT("rejected wall bounds keep the previous layout"),
			Layout.ExteriorFootprintBounds.Min.Equals(ExteriorBounds.Min, 0.1)
			&& Layout.ExteriorFootprintBounds.Max.Equals(ExteriorBounds.Max, 0.1));
		Input.ExteriorFootprintBounds = ExteriorBounds;
		Input.ExteriorFootprintBounds.Min.X = -std::numeric_limits<float>::infinity();
		TestFalse(TEXT("nonfinite wall bounds are rejected"),
			FOfficeExteriorLayoutBuilder::Build(Input, Layout));
		TestTrue(TEXT("nonfinite wall bounds keep the previous layout"),
			Layout.ExteriorFootprintBounds.Min.Equals(ExteriorBounds.Min, 0.1)
			&& Layout.ExteriorFootprintBounds.Max.Equals(ExteriorBounds.Max, 0.1));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfficeExteriorOwnBuildingHeightTest,
	"CGR.Office.Exterior.OwnBuilding.HeightClipping",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FOfficeExteriorOwnBuildingHeightTest::RunTest(const FString& Parameters)
{
	FOfficeExteriorLayoutInput Input;
	Input.TileCount = FIntPoint(1, 2);
	Input.MaxTileCount = FOfficeExteriorLayoutBuilder::GetCanonicalMaxTileCount();
	Input.CurrentFloorBounds = FOfficeFootprintGeometry::MakeFloorBounds(Input.TileCount, 400.f);
	Input.MaxFloorBounds = FOfficeFootprintGeometry::MakeFloorBounds(Input.MaxTileCount, 400.f);
	Input.TileSizeCm = 400.f;
	for (const float HeightCm : {20.f, 800.f, 4020.f, 4500.f, 14000.f})
	{
		Input.OfficeHeightCm = HeightCm;
		FOfficeExteriorLayoutResult Layout;
		if (!TestTrue(TEXT("actual height builds"), FOfficeExteriorLayoutBuilder::Build(Input, Layout))) { continue; }
		TestTrue(TEXT("visible tower never extends below the ground"),
			!Layout.bShowTowerBody || Layout.TowerBodyTransform.GetLocation().Z
				- Layout.TowerBodyTransform.GetScale3D().Z * 50.f >= Layout.GroundZ - 0.1);
		TestTrue(TEXT("facade budget holds at every height"), Layout.GetDynamicInstanceCount() <= 135);
	}
	Input.OfficeHeightCm = std::numeric_limits<float>::infinity();
	FOfficeExteriorLayoutResult Sentinel;
	Sentinel.GroundZ = 333.f;
	TestFalse(TEXT("nonfinite height is rejected"), FOfficeExteriorLayoutBuilder::Build(Input, Sentinel));
	TestEqual(TEXT("failed build keeps the previous layout"), Sentinel.GroundZ, 333.f);
	return true;
}

#endif
